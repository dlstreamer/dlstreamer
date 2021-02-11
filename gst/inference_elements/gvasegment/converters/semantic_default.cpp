/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/semantic_default.h"

#include "copy_blob_to_gststruct.h"
#include "gstgvasegment.h"
#include "inference_backend/logger.h"
#include "video_frame.h"

using namespace SegmentationPlugin;
using namespace Converters;

SemanticDefaultConverter::SemanticDefaultConverter(int show_zero_class) {
    this->show_zero_class = show_zero_class;
}

bool SemanticDefaultConverter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                       const std::vector<std::shared_ptr<InferenceFrame>> &frames,
                                       const std::string &model_name, const std::string &layer_name, GValueArray *,
                                       GstStructure *segmentation_result) {
    ITT_TASK(__FUNCTION__);
    bool flag = false;
    try {
        if (not segmentation_result)
            throw std::invalid_argument("segmentation_result tensor is nullptr");

        // Check whether we can handle this blob instead iterator
        int frame_id = 0;
        for (const auto &blob_iter : output_blobs) {
            InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is nullptr");

            const float *data = (const float *)blob->GetData();
            if (not data)
                throw std::invalid_argument("Output blob data is nullptr");

            auto dims = blob->GetDims();
            size_t dims_size = dims.size();

            static constexpr size_t min_dims_size = 3;
            if (dims_size < min_dims_size)
                throw std::invalid_argument("Output blob dimentions size " + std::to_string(dims_size) +
                                            " is not supported (less than " + std::to_string(min_dims_size) + ")");

            GstVideoInfo video_info;

            if (frame_id < 0 || (size_t)frame_id >= frames.size()) {
                break;
            }

            // This post processing happens not in main gstreamer thread (but in separate one). Thus, we can run
            // into a problem where buffer's gva_base_inference is already cleaned up (all frames are pushed from
            // decoder), so we can't use gva_base_inference's GstVideoInfo to get width and height. In this case we
            // use w and h of InferenceFrame to make VideoFrame box adding logic work correctly
            if (frames[frame_id]->gva_base_inference->info) {
                video_info = *frames[frame_id]->gva_base_inference->info;
            } else {
                video_info.width = frames[frame_id]->roi.w;
                video_info.height = frames[frame_id]->roi.h;
            }
            GVA::VideoFrame video_frame(frames[frame_id]->buffer, frames[frame_id]->info);

            GVA::Tensor tensor = video_frame.add_tensor();

            tensor.set_int("show_zero_class", show_zero_class);

            GstStructure *tensor_structure = tensor.gst_structure();
            gst_structure_set_name(tensor_structure, "semantic_segmentation");

            // make sure name="semantic_segmentation"
            assert(gst_structure_has_name(tensor_structure, "semantic_segmentation"));

            copySemanticInfoToGstStructure((const float *)blob->GetData(), dims, model_name, layer_name,
                                           blob->GetPrecision(), blob->GetLayout(), frames.size(), frame_id,
                                           tensor_structure);

            ++frame_id;
        }
        flag = true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SemanticDefault post-processing"));
    }
    return flag;
}
