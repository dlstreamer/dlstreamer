/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/instance_default.h"

#include "copy_blob_to_gststruct.h"
#include "gstgvasegment.h"
#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"
#include "video_frame.h"

using namespace SegmentationPlugin;
using namespace Converters;

bool InstanceDefaultConverter::OutputLayersName::checkBlobCorrectness(
    const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs) {
    if (!are_valid_layers_names) {
        if (!output_blobs.count(boxes))
            throw std::invalid_argument("OutputBlob must contain \"" + boxes + "\" layer");
        if (!output_blobs.count(classes))
            throw std::invalid_argument("OutputBlob must contain \"" + classes + "\" layer");
        if (!output_blobs.count(raw_masks))
            throw std::invalid_argument("OutputBlob must contain \"" + raw_masks + "\" layer");
        if (!output_blobs.count(scores))
            throw std::invalid_argument("OutputBlob must contain \"" + scores + "\" layer");
        are_valid_layers_names = true;
    }
    return are_valid_layers_names;
}

InstanceDefaultConverter::InstanceDefaultConverter(size_t height, size_t width, double threshold) {
    net_width = width;
    net_height = height;
    this->threshold = threshold;
}

bool InstanceDefaultConverter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                       const std::vector<std::shared_ptr<InferenceFrame>> &frames, const std::string &,
                                       const std::string &, GValueArray *labels_raw,
                                       GstStructure *segmentation_result) {
    ITT_TASK(__FUNCTION__);
    bool flag = false;
    try {
        if (not segmentation_result)
            throw std::invalid_argument("Segmentation_result tensor is nullptr");

        // Check whether we can handle this blob instead iterator
        layers_name.checkBlobCorrectness(output_blobs);

        // Scores layer -> FP32
        const auto &scores_blob_iter = output_blobs.find(layers_name.scores);
        const auto &scores_blob = scores_blob_iter->second;
        if (not scores_blob)
            throw std::invalid_argument("Output blob is nullptr");
        if (scores_blob->GetPrecision() != InferenceBackend::OutputBlob::Precision::FP32)
            throw std::invalid_argument("\"scores\" layer should have FP32 precision");
        const float *scores_data = (const float *)scores_blob->GetData();
        if (not scores_data)
            throw std::invalid_argument("Output blob data is nullptr");
        auto &scores_dims = scores_blob->GetDims();
        guint scores_dims_size = scores_dims.size();
        if (scores_dims_size != 1)
            throw std::invalid_argument("Output \"Scores\" Blob must have dimentions size 1 but has dimentions size " +
                                        std::to_string(scores_dims_size));

        // Boxes layer -> FP32
        const auto &box_blob_iter = output_blobs.find(layers_name.boxes);
        const auto &box_blob = box_blob_iter->second;
        if (not box_blob)
            throw std::invalid_argument("Output blob is nullptr");
        if (box_blob->GetPrecision() != InferenceBackend::OutputBlob::Precision::FP32)
            throw std::invalid_argument("\"boxes\" layer should have FP32 precision");
        const float *box_data = (const float *)box_blob->GetData();
        if (not box_data)
            throw std::invalid_argument("Output blob data is nullptr");
        auto &box_dims = box_blob->GetDims();
        guint box_dims_size = box_dims.size();
        if (box_dims_size != 2)
            throw std::invalid_argument("Output \"Box\" Blob must have dimentions size 2 but has dimentions size " +
                                        std::to_string(box_dims_size));
        if (scores_dims[0] != box_dims[0])
            throw std::invalid_argument("Invalid max objects number");

        // Classes layer -> I32
        const auto &classes_blob_iter = output_blobs.find(layers_name.classes);
        const auto &classes_blob = classes_blob_iter->second;
        if (not classes_blob)
            throw std::invalid_argument("Output blob is nullptr");
        if (classes_blob->GetPrecision() != InferenceBackend::OutputBlob::Precision::I32)
            throw std::invalid_argument("\"raw_masks\" layer should have I32 precision");
        const int *classes_data = (const int *)classes_blob->GetData();
        if (not classes_data)
            throw std::invalid_argument("Output blob data is nullptr");
        auto &classes_dims = classes_blob->GetDims();
        guint classes_dims_size = classes_dims.size();
        if (classes_dims_size != 1)
            throw std::invalid_argument("Output \"Classes\" Blob must have dimentions size 1 but has dimentions size " +
                                        std::to_string(classes_dims_size));
        if (scores_dims[0] != classes_dims[0])
            throw std::invalid_argument("Invalid max objects number");

        // Raw_masks layer -> FP32
        const auto &raw_masks_blob_iter = output_blobs.find(layers_name.raw_masks);
        const auto &raw_masks_blob = raw_masks_blob_iter->second;
        if (not raw_masks_blob)
            throw std::invalid_argument("Output blob is nullptr");
        if (raw_masks_blob->GetPrecision() != InferenceBackend::OutputBlob::Precision::FP32)
            throw std::invalid_argument("\"raw_masks\" layer should have FP32 precision");
        const float *raw_masks_data = (const float *)raw_masks_blob->GetData();
        if (not raw_masks_data)
            throw std::invalid_argument("Output blob data is nullptr");
        auto &raw_masks_dims = raw_masks_blob->GetDims();
        guint raw_masks_dims_size = raw_masks_dims.size();
        if (raw_masks_dims_size != 4)
            throw std::invalid_argument(
                "Output \"Raw_mask\" Blob must have dimentions size 4 but has dimentions size " +
                std::to_string(raw_masks_dims_size));
        if (scores_dims[0] != raw_masks_dims[0])
            throw std::invalid_argument("Invalid max objects number");

        // VideoFrame preporations
        GstVideoInfo video_info;
        size_t frame_id = 0;
        if (frame_id >= frames.size()) {
            return flag;
        }
        if (frames[frame_id]->gva_base_inference->info) {
            video_info = *frames[frame_id]->gva_base_inference->info;
        } else {
            video_info.width = frames[frame_id]->roi.w;
            video_info.height = frames[frame_id]->roi.h;
        }
        GVA::VideoFrame video_frame(frames[frame_id]->buffer, frames[frame_id]->info);

        for (int detected_object = 0; detected_object < (int)scores_dims[0]; ++detected_object) {
            if (scores_data[detected_object] > threshold) {
                double x = box_data[4 * detected_object + 0] / net_width;
                double y = box_data[4 * detected_object + 1] / net_height;
                double w = (box_data[4 * detected_object + 2] - box_data[4 * detected_object + 0]) / net_width;
                double h = (box_data[4 * detected_object + 3] - box_data[4 * detected_object + 1]) / net_height;

                int class_id = classes_data[detected_object];
                int mask_height = raw_masks_dims[2];
                int mask_width = raw_masks_dims[3];
                int object_size = raw_masks_dims[1] * raw_masks_dims[2] * raw_masks_dims[3];
                std::string class_label;
                getLabelByLabelId(labels_raw, class_id, class_label);
                GVA::RegionOfInterest roi =
                    video_frame.add_region(x, y, w, h, class_label, scores_data[detected_object], true);

                GVA::Tensor tensor = roi.add_tensor("instance_segmentation");
                tensor.set_uint("class_id", class_id);
                tensor.set_uint("mask_height", raw_masks_dims[2]);
                tensor.set_uint("mask_width", raw_masks_dims[3]);

                size_t data_shift = (object_size * detected_object + class_id * mask_height * mask_width);
                size_t size = mask_height * mask_width * sizeof(float);
                copy_buffer_to_structure(tensor.gst_structure(), raw_masks_data + data_shift, size);
            }
        }

        flag = true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do Semantic_Args_Plane_Max post-processing"));
    }
    return flag;
}
