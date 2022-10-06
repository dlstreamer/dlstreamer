/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/cpu/elements/tensor_postproc_detection.h"
#include "dlstreamer/base/transform.h"
#include "dlstreamer/cpu/utils.h"
#include "dlstreamer/image_metadata.h"
#include "dlstreamer/utils.h"
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
    TensorPostProcDetection(DictionaryCPtr params, const ContextPtr &app_context) : BaseTransformInplace(app_context) {
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

        if (_box_index < 0) {
            DLS_CHECK(auto_detect_format(frame_info(src)));
        }

        int num_tensors = frame->num_tensors();
        DLS_CHECK(_box_index < num_tensors && _box_index >= 0);
        DLS_CHECK(_confidence_index < num_tensors);
        DLS_CHECK(_label_index < num_tensors);
        DLS_CHECK(_imageid_index < num_tensors);
        DLS_CHECK(_mask_index < num_tensors);

        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*src);
        int batch_index = source_id_meta ? source_id_meta->batch_index() : 0;

        auto model_info_meta = find_metadata<ModelInfoMetadata>(*src);
        auto model_name = model_info_meta ? model_info_meta->model_name() : std::string();
        auto model_input_layers = model_info_meta ? model_info_meta->input_layers() : std::vector<std::string>();

        auto label_dtype = (_label_index >= 0) ? frame->tensor(_label_index)->info().dtype : DataType(0);

        // all layers should have same "num_objects"
        size_t num_objects = get_num_objects(frame->tensor(0)->info());
        for (int i = 1; i < num_tensors; i++)
            DLS_CHECK(num_objects == get_num_objects(frame->tensor(i)->info()));

        for (size_t i = 0; i < num_objects; i++) {
            if (_imageid_index >= 0) {
                float imageid = *frame->tensor(_imageid_index)->data<float>({i, _imageid_offset}, false);
                if (imageid != batch_index)
                    continue;
                if (imageid < 0)
                    break;
            }

            float confidence = 0;
            if (_confidence_index >= 0) {
                confidence = *frame->tensor(_confidence_index)->data<float>({i, _confidence_offset}, false);
                if (confidence < _threshold)
                    continue;
            }

            int64_t label_id = -1;
            if (_label_index >= 0) {
                auto label_tensor = frame->tensor(_label_index);
                std::vector<size_t> offset{i, _label_offset};
                if (label_tensor->info().shape.size() == 1) {
                    if (_label_offset != 0)
                        throw std::runtime_error("Invalid label offset");
                    offset.resize(1);
                }

                if (label_dtype == DataType::Float32)
                    label_id = *label_tensor->data<float>(offset, false);
                else if (label_dtype == DataType::Int32)
                    label_id = *label_tensor->data<int32_t>(offset, false);
                else if (label_dtype == DataType::Int64)
                    label_id = *label_tensor->data<int64_t>(offset, false);
                else
                    throw std::runtime_error("Unsupported data type in label tensor");
            }

            float *box = frame->tensor(_box_index)->data<float>({i, _box_offset}, false);
            float x_min = box[0];
            float y_min = box[1];
            float x_max = box[2];
            float y_max = box[3];
            if (!(x_min < 2 && y_min < 2 && x_max < 2 && y_max < 2)) {
                // convert absolute coordinates to normalized coordinates in range [0, 1]
                DLS_CHECK(model_info_meta);
                auto input = model_info_meta->input();
                ImageInfo info(input.tensors.front());
                x_min /= info.width();
                y_min /= info.height();
                x_max /= info.width();
                y_max /= info.height();
            }

            DetectionMetadata meta(src->metadata().add(DetectionMetadata::name));
            std::string label = (label_id >= 0 && label_id < (int)_labels.size()) ? _labels[label_id] : std::string();
            meta.init(x_min, y_min, x_max, y_max, confidence, label_id, label);
            if (!model_name.empty())
                meta.set_model_name(model_name);

            if (_mask_index >= 0) {
                auto mask_data = get_tensor_slice(frame->tensor(_mask_index), {{i, 1}}, true);
                auto layer_name = (_mask_index < (int)model_input_layers.size()) ? model_input_layers[_mask_index] : "";
                meta.init_tensor_data(*mask_data, layer_name, "mask");
            }
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
    bool auto_detect_format(const FrameInfo &info) {
        size_t num_tensors = info.tensors.size();
        auto &info0 = info.tensors[0];
        auto shape0 = info0.shape;
        if (num_tensors == 1 && shape0[shape0.size() - 1] == 7) {
            _box_index = 0;
            _confidence_index = 0;
            _label_index = 0;
            _imageid_index = 0;

            _box_offset = 3;
            _confidence_offset = 2;
            _label_offset = 1;
            _imageid_offset = 0;

            return true;
        }
        if (num_tensors == 2 && shape0[shape0.size() - 1] == 5) {
            shape0.pop_back();
            if (shape0 == info.tensors[1].shape) {
                _box_index = 0;
                _confidence_index = 0;
                _label_index = 1;

                _box_offset = 0;
                _confidence_offset = 4;
                _label_offset = 0;

                return true;
            }
        }
        if (num_tensors == 3) {
            for (size_t tensorIdx = 0; tensorIdx < info.tensors.size(); ++tensorIdx) {
                if (info.tensors[tensorIdx].shape.size() == 1)
                    _label_index = tensorIdx;
                else if (info.tensors[tensorIdx].shape.size() == 2)
                    _box_index = _confidence_index = tensorIdx;
                else if (info.tensors[tensorIdx].shape.size() == 3)
                    _mask_index = tensorIdx;
                else
                    return false;
            }
            _num_objects_index = 0;
            _box_offset = 0;
            _confidence_offset = 4;
            _label_offset = 0;
            return true;
        }
        return false;
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
};

extern "C" {
ElementDesc tensor_postproc_detection = {
    .name = "tensor_postproc_detection",
    .description =
        "Post-processing of object detection inference to extract bounding box coordinates, confidence, label, mask",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = {MediaType::Tensors},
    .output_info = {MediaType::Tensors},
    .create = create_element<TensorPostProcDetection>,
    .flags = 0};
}

} // namespace dlstreamer
