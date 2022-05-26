/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "detection_output.h"
#include "dlstreamer/metadata.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

namespace param {
static constexpr auto labels = "labels";
static constexpr auto threshold = "threshold";

static constexpr auto default_threshold = 0.5;
}; // namespace param

static ParamDescVector params_desc = {
    {param::labels, "Comma-separated list of object classes", ""},
    {param::threshold,
     "Detection threshold - only objects with confidence values above the threshold will be added to the frame",
     param::default_threshold, 0.0, 1.0}};

class PostProcDetectionOutput : public TransformInplace {
    static const size_t min_dims_size = 2;
    static const size_t last_dim = 7; // OV DetectionOutput format
    std::vector<std::string> _labels;
    float _confidence_threshold;

  public:
    PostProcDetectionOutput(ITransformController &transform_ctrl, DictionaryCPtr params)
        : TransformInplace(transform_ctrl, std::move(params)) {
        _labels = split_string(_params->get<std::string>(param::labels, ""), ',');
        _confidence_threshold = _params->get<double>(param::threshold, param::default_threshold);
    }

    void set_info(const BufferInfo &input_info, const BufferInfo &output_info) override {
        if (input_info.planes[0].shape != output_info.planes[0].shape)
            throw std::runtime_error("Expect same tensor shape on input and output");
        _in_mapper = _transform_ctrl->create_input_mapper(BufferType::CPU);
    }

    bool process(BufferPtr src) override {
        auto src_cpu = _in_mapper->map(src, AccessMode::READ);
        float *data = static_cast<float *>(src_cpu->data());
        auto dims = src->info()->planes[0].shape;
        size_t dims_size = dims.size();

        if (dims_size < min_dims_size)
            throw std::invalid_argument("Expect tensor rank >= " + std::to_string(min_dims_size));
        for (size_t i = min_dims_size + 1; i < dims_size; ++i) {
            if (dims[dims_size - i] != 1)
                throw std::invalid_argument("Expect dimension " + std::to_string(i) + " to be equal 1");
        }
        if (dims[dims_size - 1] != last_dim)
            throw std::invalid_argument("Expect last dimension to be equal " + std::to_string(last_dim));

        auto source_id_meta = find_metadata<SourceIdentifierMetadata>(*src);
        int batch_index = source_id_meta ? source_id_meta->batch_index() : 0;

        size_t max_proposal_count = dims[dims_size - 2];
        for (size_t i = 0; i < max_proposal_count; ++i) {
            int image_id = static_cast<int>(data[i * last_dim + 0]);
            float label_id = data[i * last_dim + 1];
            float confidence = data[i * last_dim + 2];
            float x_min = data[i * last_dim + 3];
            float y_min = data[i * last_dim + 4];
            float x_max = data[i * last_dim + 5];
            float y_max = data[i * last_dim + 6];

            if (image_id != batch_index)
                continue;
            if (image_id < 0)
                break;

            if (confidence < _confidence_threshold) {
                continue;
            }

            auto meta = DetectionMetadata(src->add_metadata(DetectionMetadata::name));
            auto label = _labels.empty() ? std::string() : _labels[label_id];
            meta.init(x_min, y_min, x_max, y_max, confidence, label_id, label);
        }

        return true;
    }

  private:
    BufferMapperPtr _in_mapper;
};

TransformDesc PostProcDetectionOutputDesc = {
    .name = "tensor_postproc_detection_output",
    .description = "Post-processing of object detection inference to extract bounding box list",
    .author = "Intel Corporation",
    .params = &params_desc,
    .input_info = {MediaType::TENSORS},
    .output_info = {MediaType::TENSORS},
    .create = TransformBase::create<PostProcDetectionOutput>,
    .flags = TRANSFORM_FLAG_SUPPORT_PARAMS_STRUCTURE};

} // namespace dlstreamer
