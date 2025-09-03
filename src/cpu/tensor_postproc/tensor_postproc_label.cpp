/*******************************************************************************
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_postproc_label.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/memory_mapper_factory.h"
#include "load_labels_file.h"
#include <cmath>

namespace dlstreamer {

namespace param {
static constexpr auto method = "method";
static constexpr auto labels = "labels";
static constexpr auto labels_file = "labels-file";
static constexpr auto threshold = "threshold";
static constexpr auto compound_threshold = "compound-threshold";
static constexpr auto attribute_name = "attribute_name";
static constexpr auto layer_name = "layer-name"; // (string)
} // namespace param

namespace dflt {
static constexpr auto method = "max";
static constexpr auto threshold = 0.0;
static constexpr auto compound_threshold = 0.5;
} // namespace dflt

static ParamDescVector params_desc = {
    {param::method, "Method used to post-process tensor data", dflt::method, {"max", "softmax", "compound", "index"}},
    {param::labels, "Array of object classes", std::vector<std::string>()},
    {param::labels_file, "Path to .txt file containing object classes (one per line)", std::string()},
    {param::attribute_name, "Name for metadata created and attached by this element", std::string()},
    {param::layer_name, "Name of output layer to process (in case of multiple output tensors)", std::string()},
    {param::threshold, "Threshold for confidence values", dflt::threshold, 0.0, 1.0},
    {param::compound_threshold, "Threshold for compound method", dflt::compound_threshold, 0.0, 1.0},
};

class PostProcLabel : public BaseTransformInplace {
  public:
    enum class Method { Max, SoftMax, Compound, Index };

    PostProcLabel(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
        _method = method_from_string(params->get<std::string>(param::method, dflt::method));
        _labels = params->get(param::labels, std::vector<std::string>());
        auto labels_file = params->get(param::labels_file, std::string());
        if (!labels_file.empty())
            _labels = load_labels_file(labels_file);
        DLS_CHECK(!_labels.empty())

        _attribute_name = params->get(param::attribute_name, std::string());
        _layer_name = params->get<std::string>(param::layer_name);
        _threshold = params->get(param::threshold, dflt::threshold);
        _compound_threshold = params->get(param::compound_threshold, dflt::threshold);
    }

    void set_info(const FrameInfo &info) override {
        BaseTransformInplace::set_info(info);

        if (_layer_name.empty()) {
            DLS_CHECK(info.tensors.size() == 1)
        }
    }

    bool process(FramePtr frame) override {
        TensorPtr tensor;
        auto src = frame.map(AccessMode::Read);

        if (_layer_index < 0) {
            detect_layer_index(frame);
        }
        tensor = src->tensor(_layer_index);
        float *data = tensor->data<float>();
        const auto data_size = tensor->info().size();
        DLS_CHECK(data)

        double confidence = -1;
        int label_id = -1;
        std::string label;

        switch (_method) {
        case Method::Max:
            run_max(data, data_size, label, confidence, label_id);
            break;
        case Method::SoftMax:
            run_soft_max(data, data_size, label, confidence, label_id);
            break;
        case Method::Compound:
            run_compound(data, data_size, label, confidence);
            break;
        case Method::Index:
            run_index(data, data_size, label);
            break;
        default:
            throw std::runtime_error("Unknown method");
        }

        if (confidence >= _threshold || confidence == -1) {
            auto meta = add_metadata<ClassificationMetadata>(*frame);
            if (!_attribute_name.empty())
                meta.set_name(_attribute_name);
            if (!_model_name.empty())
                meta.set_model_name(_model_name);
            if (confidence >= 0)
                meta.set_confidence(confidence);
            if (label_id != -1)
                meta.set_label_id(label_id);
            if (!label.empty()) {
                meta.set_label(label);
            }
        }

        return true;
    }

  protected:
    static PostProcLabel::Method method_from_string(const std::string &method_string) {
        static const std::pair<std::string_view, Method> map_array[] = {{"max", Method::Max},
                                                                        {"softmax", Method::SoftMax},
                                                                        {"compound", Method::Compound},
                                                                        {"index", Method::Index}};
        for (const auto &item : map_array) {
            if (item.first == method_string)
                return item.second;
        }
        throw std::runtime_error(std::string("Unsupported method: ") + method_string);
    }

    void run_max(const float *data, size_t size, std::string &label, double &confidence, int &label_id) const {
        auto max_elem = std::max_element(data, data + size);
        label_id = std::distance(data, max_elem);
        label = _labels.at(label_id);
        confidence = *max_elem;
    }

    void run_soft_max(const float *data, size_t size, std::string &label, double &confidence, int &label_id) const {
        const float max_confidence = *std::max_element(data, data + size);
        std::vector<float> sftm_arr(size);
        float sum = 0;
        for (size_t i = 0; i < size; ++i) {
            sftm_arr[i] = std::exp(data[i] - max_confidence);
            sum += sftm_arr.at(i);
        }
        if (sum > 0) {
            for (size_t i = 0; i < size; ++i) {
                sftm_arr[i] /= sum;
            }
        }
        auto max_elem = std::max_element(sftm_arr.begin(), sftm_arr.end());
        label_id = std::distance(sftm_arr.begin(), max_elem);
        label = _labels.at(label_id);
        confidence = *max_elem; // TODO normalized confidence (*max_elem) or original confidence (data[label_id])?
    }

    void run_compound(const float *data, size_t size, std::string &label, double &confidence) const {
        confidence = 0;
        for (size_t j = 0; j < size; j++) {
            std::string result_label;
            if (data[j] >= _compound_threshold) {
                result_label = _labels.at(j * 2);
            } else if (data[j] > 0) {
                result_label = _labels.at(j * 2 + 1);
            }
            if (!result_label.empty()) {
                if (!label.empty() && !isspace(label.back()))
                    label += " ";
                label += result_label;
            }
            if (data[j] >= confidence)
                confidence = data[j];
        }
    }

    void run_index(const float *data, size_t size, std::string &label) const {
        for (size_t j = 0; j < size; j++) {
            int value = static_cast<int>(data[j]);
            if (value < 0 || static_cast<size_t>(value) >= _labels.size())
                break;
            label += _labels.at(value);
        }
    }

  private:
    void detect_layer_index(FramePtr frame) {
        auto model_info = find_metadata<ModelInfoMetadata>(*frame);
        if (model_info)
            _model_name = model_info->model_name();

        if (!_layer_name.empty()) {
            if (!model_info)
                throw std::runtime_error("Layer name specified but model info not found");
            auto lnames = model_info->output_layers();
            auto it = std::find(lnames.cbegin(), lnames.cend(), _layer_name);
            if (it == lnames.end())
                throw std::runtime_error("There's no output layer with name:" + _layer_name);
            _layer_index = std::distance(lnames.cbegin(), it);
        } else
            _layer_index = 0;

        if (_method != Method::Index) {
            size_t expected_labels_count = _info.tensors[_layer_index].size();
            if (_method == Method::Compound)
                expected_labels_count *= 2;
            if (_labels.size() > expected_labels_count) {
                throw std::invalid_argument("Wrong number of object classes");
            }
        }
    }

  protected:
    Method _method = Method::Max;
    std::vector<std::string> _labels;
    std::string _attribute_name;
    std::string _layer_name;
    double _threshold;
    double _compound_threshold;
    int _layer_index = -1;
    std::string _model_name;
};

extern "C" {
ElementDesc tensor_postproc_label = {.name = "tensor_postproc_label",
                                     .description =
                                         "Post-processing of classification inference to extract object classes",
                                     .author = "Intel Corporation",
                                     .params = &params_desc,
                                     .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                     .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
                                     .create = create_element<PostProcLabel>,
                                     .flags = 0};
}

} // namespace dlstreamer
