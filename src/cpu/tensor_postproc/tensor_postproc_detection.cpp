/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_postproc_detection.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"
#include "dlstreamer_logger.h"
#include "load_labels_file.h"

namespace dlstreamer {

namespace param {
static constexpr auto labels = "labels";
static constexpr auto labels_file = "labels-file";
static constexpr auto threshold = "threshold";

static constexpr auto box_index = "box_index";
static constexpr auto confidence_index = "confidence_index";
static constexpr auto label_index = "label_index";
static constexpr auto imageid_index = "imageid_index";
static constexpr auto mask_index = "mask_index";

static constexpr auto box_offset = "box_offset";
static constexpr auto confidence_offset = "confidence_offset";
static constexpr auto label_offset = "label_offset";
static constexpr auto imageid_offset = "imageid_offset";

static constexpr auto default_threshold = 0.5;
}; // namespace param

static ParamDescVector params_desc = {
    {param::labels, "Array of object classes", std::vector<std::string>()},
    {param::labels_file, "Path to .txt file containing object classes (one per line)", std::string()},
    {param::threshold,
     "Detection threshold - only objects with confidence values above the threshold will be added to the frame",
     param::default_threshold, 0.0, 1.0},

    {param::box_index, "Index of layer containing bounding box data", -1, -1, INT32_MAX},
    {param::confidence_index, "Index of layer containing confidence data", -1, -1, INT32_MAX},
    {param::label_index, "Index of layer containing label data", -1, -1, INT32_MAX},
    {param::imageid_index, "Index of layer containing imageid data", -1, -1, INT32_MAX},
    {param::mask_index, "Index of layer containing mask data", -1, -1, INT32_MAX},

    {param::box_offset, "Offset inside layer containing bounding box data", -1, -1, INT32_MAX},
    {param::confidence_offset, "Offset inside layer containing confidence data", -1, -1, INT32_MAX},
    {param::label_offset, "Offset inside layer containing label data", -1, -1, INT32_MAX},
    {param::imageid_offset, "Offset inside layer containing imageid data", -1, -1, INT32_MAX},
};

class TensorPostProcDetection : public BaseTransformInplace {
  public:
    TensorPostProcDetection(DictionaryCPtr params, const ContextPtr &app_context)
        : BaseTransformInplace(app_context),
          _logger(log::get_or_nullsink(params->get(param::logger_name, std::string()))) {
        SPDLOG_LOGGER_INFO(_logger, "create element");
        _labels = params->get(param::labels, std::vector<std::string>());
        auto labels_file = params->get(param::labels_file, std::string());
        if (!labels_file.empty())
            _labels = load_labels_file(labels_file);
        _threshold = params->get<double>(param::threshold, param::default_threshold);

        _box_index = params->get(param::box_index, -1);
        _confidence_index = params->get(param::confidence_index, -1);
        _label_index = params->get(param::label_index, -1);
        _imageid_index = params->get(param::imageid_index, -1);
        _mask_index = params->get(param::mask_index, -1);

        _box_offset = params->get(param::box_offset, -1);
        _confidence_offset = params->get(param::confidence_offset, -1);
        _label_offset = params->get(param::label_offset, -1);
        _imageid_offset = params->get(param::imageid_offset, -1);
    }

    bool process(FramePtr src) override {
        auto frame = src.map(AccessMode::Read);
        DLS_CHECK(auto_detect_format(src));
        parse_model_info(src);

        int num_tensors = frame->num_tensors();
        DLS_CHECK(_box_index < num_tensors && _box_index >= 0);
        DLS_CHECK(_confidence_index < num_tensors);
        DLS_CHECK(_label_index < num_tensors);
        DLS_CHECK(_imageid_index < num_tensors);
        DLS_CHECK(_mask_index < num_tensors);

        // all layers should have same "num_objects"
        size_t num_objects = get_num_objects(frame->tensor(_box_index)->info());
        DLS_CHECK(check_objects_equal(frame, num_objects, _confidence_index));
        DLS_CHECK(check_objects_equal(frame, num_objects, _label_index));
        DLS_CHECK(check_objects_equal(frame, num_objects, _imageid_index));
        DLS_CHECK(check_objects_equal(frame, num_objects, _mask_index));

        int batch_index = 0;
        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*src);
        if (source_id_meta)
            batch_index = source_id_meta->batch_index();

        std::vector<size_t> confidence_offset;
        std::vector<size_t> label_offset;
        std::vector<size_t> box_offset{0, _box_offset};

        if (_confidence_index >= 0) {
            auto confidence_tensor = frame->tensor(_confidence_index);
            confidence_offset = (confidence_tensor->info().shape.size() == 1)
                                    ? std::vector<size_t>(1)
                                    : std::vector<size_t>{0, _confidence_offset};
        }
        if (_label_index >= 0) {
            auto label_tensor = frame->tensor(_label_index);
            label_offset = (label_tensor->info().shape.size() == 1) ? std::vector<size_t>(1)
                                                                    : std::vector<size_t>{0, _label_offset};
        }

        for (size_t object_idx = 0; object_idx < num_objects; object_idx++) {
            if (_imageid_index >= 0) {
                float imageid = *frame->tensor(_imageid_index)->data<float>({object_idx, _imageid_offset}, false);
                if (imageid != batch_index)
                    continue;
                if (imageid < 0)
                    break;
            }

            float confidence = 0;
            if (_confidence_index >= 0) {
                confidence_offset.at(0) = object_idx;
                confidence = *frame->tensor(_confidence_index)->data<float>(confidence_offset, false);
                if (confidence < _threshold)
                    continue;
            }

            int64_t label_id = -1;
            if (_label_index >= 0) {
                auto label_tensor = frame->tensor(_label_index);
                label_offset.at(0) = object_idx;
                auto label_dtype = frame->tensor(_label_index)->info().dtype;
                if (label_dtype == DataType::Float32)
                    label_id = *label_tensor->data<float>(label_offset, false);
                else if (label_dtype == DataType::Int32)
                    label_id = *label_tensor->data<int32_t>(label_offset, false);
                else if (label_dtype == DataType::Int64)
                    label_id = *label_tensor->data<int64_t>(label_offset, false);
                else
                    throw std::runtime_error("Unsupported data type in label tensor");
            }

            box_offset.at(0) = object_idx;
            float *box = frame->tensor(_box_index)->data<float>(box_offset, false);
            float x_min = box[0];
            float y_min = box[1];
            float x_max = box[2];
            float y_max = box[3];
            if (!(x_min < 2 && y_min < 2 && x_max < 2 && y_max < 2)) { // TODO is this check robust?
                // convert absolute coordinates to normalized coordinates in range [0, 1]
                DLS_CHECK(_model_input_width && _model_input_height);
                x_min /= _model_input_width;
                y_min /= _model_input_height;
                x_max /= _model_input_width;
                y_max /= _model_input_height;
            }

            DetectionMetadata meta(src->metadata().add(DetectionMetadata::name));
            std::string label = (label_id >= 0 && label_id < (int)_labels.size()) ? _labels[label_id] : std::string();
            meta.init(x_min, y_min, x_max, y_max, confidence, label_id, label);

            if (_mask_index >= 0) {
                auto mask_data = get_tensor_slice(frame->tensor(_mask_index), {{object_idx, 1}}, true);
                meta.init_tensor_data(*mask_data, std::string(), "mask");
            }

            if (!_model_name.empty())
                meta.set_model_name(_model_name);
            if (!_layer_name.empty())
                meta.set_layer_name(_layer_name);
        }

        return true;
    }

  private:
    size_t get_num_objects(const TensorInfo &info) const {
        if (_num_objects_index >= 0)
            return info.shape[_num_objects_index];
        size_t tsize = info.size();
        if (info.shape.size() > 1)
            tsize /= info.shape[info.shape.size() - 1];
        return tsize;
    }
    bool check_objects_equal(const FramePtr &frame, size_t num_objects, int index) {
        if (index < 0)
            return true;
        return (num_objects == get_num_objects(frame->tensor(index)->info()));
    }

    bool auto_detect_format(const FramePtr &frame) {
        if (_box_index >= 0)
            return true;
        FrameInfo info = frame_info(frame);
        size_t num_tensors = info.tensors.size();
        auto &info0 = info.tensors[0];
        auto shape0 = info0.shape;
        if (num_tensors == 1 && shape0.back() == 5) {
            _box_index = 0;
            _confidence_index = 0;

            _box_offset = 0;
            _confidence_offset = 4;
        } else if (num_tensors == 1 && shape0.back() == 7) {
            _box_index = 0;
            _confidence_index = 0;
            _label_index = 0;
            _imageid_index = 0;

            _box_offset = 3;
            _confidence_offset = 2;
            _label_offset = 1;
            _imageid_offset = 0;
        } else if (num_tensors == 2 && shape0.back() == 5) {
            shape0.pop_back();
            if (shape0 == info.tensors[1].shape) {
                _box_index = 0;
                _confidence_index = 0;
                _label_index = 1;

                _box_offset = 0;
                _confidence_offset = 4;
                _label_offset = 0;
            }
        } else if ((num_tensors == 3) || (num_tensors == 5)) {
            for (size_t tensorIdx = 0; tensorIdx < 3; ++tensorIdx) {
                const auto &current_tensor = info.tensors[tensorIdx];
                switch (current_tensor.shape.size()) {
                case 1:
                    if (current_tensor.dtype == DataType::Float32) {
                        _confidence_index = tensorIdx;
                        _confidence_offset = 0;
                    } else {
                        _label_index = tensorIdx;
                    }
                    break;
                case 2:
                    if (current_tensor.shape[1] == 5) {
                        _confidence_index = tensorIdx;
                        _confidence_offset = 4;
                    }
                    _box_index = tensorIdx;
                    break;
                case 3:
                    _mask_index = tensorIdx;
                    break;
                default:
                    SPDLOG_LOGGER_ERROR(_logger, "unsupported shape size: {} on tensor_idx: {}",
                                        current_tensor.shape.size(), tensorIdx);
                    _box_index = -1;
                    return false;
                }
            }
            _num_objects_index = 0;
            _box_offset = 0;
            _label_offset = 0;
        }
        if (_box_index >= 0) {
            SPDLOG_LOGGER_INFO(_logger, "Params detected for num_tensors: {} tensor_info: {}", num_tensors,
                               tensor_info_to_string(info0));
            SPDLOG_LOGGER_INFO(_logger,
                               "Params pairs index and offset : box: {}x{}, confidence: {}x{}, label: {}x{}, "
                               "image: {}x{} _num_objects_index: {} _mask_index: {}",
                               _box_index, _box_offset, _confidence_index, _confidence_offset, _label_index,
                               _label_offset, _imageid_index, _imageid_offset, _num_objects_index, _mask_index);
            return true;
        } else {
            SPDLOG_LOGGER_ERROR(_logger, "unsupported num_tensors: {} tensor_info: {}", num_tensors,
                                tensor_info_to_string(info0));
            return false;
        }
    }

    void parse_model_info(const FramePtr &frame) {
        if (!_model_name.empty())
            return;
        auto model_info_meta = find_metadata<ModelInfoMetadata>(*frame);
        if (!model_info_meta)
            return;

        _model_name = model_info_meta->model_name();

        auto output_layers = model_info_meta->output_layers();
        if (_mask_index >= 0 && _mask_index < (int)output_layers.size()) {
            _layer_name = output_layers[_mask_index];
        } else {
            _layer_name = join_strings(output_layers.cbegin(), output_layers.cend(), '\\');
        }

        FrameInfo input_info = model_info_meta->input();
        if (input_info.tensors.size()) {
            ImageInfo image_info(input_info.tensors.front());
            auto layout = image_info.layout();
            if (layout.w_position() >= 0 && layout.h_position() >= 0) {
                _model_input_width = image_info.width();
                _model_input_height = image_info.height();
            }
        }
    }

  protected:
    std::vector<std::string> _labels;
    float _threshold;

    int _num_objects_index = -1;
    int _box_index;
    int _confidence_index;
    int _label_index;
    int _imageid_index;
    int _mask_index;
    size_t _box_offset;
    size_t _confidence_offset;
    size_t _label_offset;
    size_t _imageid_offset;

    // From ModelInfoMetadata
    std::string _model_name;
    std::string _layer_name;
    int _model_input_width = 0;
    int _model_input_height = 0;

    std::shared_ptr<spdlog::logger> _logger;
};

extern "C" {
ElementDesc tensor_postproc_detection = {
    .name = "tensor_postproc_detection",
    .description =
        "Post-processing of object detection inference to extract bounding box coordinates, confidence, label, mask",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
    .output_info = MAKE_FRAME_INFO_VECTOR({MediaType::Tensors}),
    .create = create_element<TensorPostProcDetection>,
    .flags = 0};
}

} // namespace dlstreamer
