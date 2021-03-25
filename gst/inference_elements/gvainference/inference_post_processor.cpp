/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_post_processor.h"

#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "inference_backend/safe_arithmetic.h"
#include "inference_impl.h"

#include "copy_blob_to_gststruct.h"

#include <gst/gst.h>

#include <string>

#ifndef __PRETTY_FUNCTION__
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

using namespace InferenceBackend;
using namespace InferencePlugin;

namespace {

inline bool sameRegion(const GstVideoRegionOfInterestMeta *left, const GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

GstVideoRegionOfInterestMeta *findROIMeta(const InferenceFrame *frame) {
    GstBuffer *buffer = frame->buffer;
    if (not buffer)
        throw std::invalid_argument("Inference frame's buffer is nullptr");
    const GstVideoRegionOfInterestMeta *frame_roi = &frame->roi;
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, frame_roi)) {
            return meta;
        }
    }
    return meta;
}

void attachROIResult(InferenceFrame *frame, OutputBlob::Ptr &blob, const std::string &model_name,
                     const std::string &layer_name, size_t batch_size, size_t frame_index) {
    /* creates inference results */
    GstVideoRegionOfInterestMeta *meta = findROIMeta(frame);
    if (!meta) {
        GST_WARNING("No detection tensors were found for this buffer in case of roi-list inference");
        return;
    }
    /* initializes inference results */
    const std::string struct_name = "layer:" + layer_name;
    GstStructure *result = gst_structure_new_empty(struct_name.c_str());
    CopyOutputBlobToGstStructure(blob, result, model_name.c_str(), layer_name.c_str(), batch_size, frame_index);
    /* attach GstStructure with inference results to detection meta */
    gst_video_region_of_interest_meta_add_param(meta, result);
    /* store inference to update inference history when pushing output buffers */
    frame->roi_classifications.push_back(result);
}

void attachFullFrameResult(InferenceFrame *frame, OutputBlob::Ptr &blob, const std::string &model_name,
                           const std::string &layer_name, size_t batch_size, size_t frame_index, size_t blob_id) {
    /* creates inference results */

    gva_buffer_check_and_make_writable(&frame->buffer, __PRETTY_FUNCTION__);

    GstGVATensorMeta *tensor = GST_GVA_TENSOR_META_ADD(frame->buffer);
    if (not tensor) {
        throw std::runtime_error("Failed to add GstGVATensorMeta instance");
    }

    /* initializes inference results */
    const std::string struct_name = "layer:" + layer_name;
    gst_structure_set_name(tensor->data, struct_name.c_str());
    if (not gst_structure_has_name(tensor->data, struct_name.c_str())) {
        throw std::invalid_argument("Failed to set '" + struct_name + "' as GstStructure name");
    }
    CopyOutputBlobToGstStructure(blob, tensor->data, model_name.c_str(), layer_name.c_str(), batch_size, frame_index);
    // In different versions of GStreamer, metas are attached to the buffer in a different order. Thus,
    // we identify our meta using tensor_id.
    gst_structure_set(tensor->data, "tensor_id", G_TYPE_INT, safe_convert<int>(blob_id), "element_id", G_TYPE_STRING,
                      frame->gva_base_inference->model_instance_id, NULL);
}

} // anonymous namespace

InferencePostProcessor::InferencePostProcessor(const InferenceImpl *inference_impl) {
    if (inference_impl == nullptr)
        throw std::runtime_error("InferencePostProcessor could not be initialized with empty InferenceImple");
    auto &models = inference_impl->GetModels();
    if (models.size() == 0)
        return;
    if (models.size() > 1)
        throw std::runtime_error("Multimodels is not supported");
    model_name = models.front().name;
}

PostProcessor::ExitStatus
InferencePostProcessor::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                                std::vector<std::shared_ptr<InferenceFrame>> &frames) {
    try {
        if (frames.empty()) {
            throw std::invalid_argument("There are no inference frames");
        }

        size_t blob_id = 0;
        for (const auto &blob_iter : output_blobs) {
            OutputBlob::Ptr blob = blob_iter.second;
            if (not blob) {
                throw std::invalid_argument("Output blob is empty");
            }
            const std::string layer_name = blob_iter.first;

            for (size_t i = 0; i < frames.size(); ++i) {
                if (frames[i]->gva_base_inference->inference_region == FULL_FRAME) {
                    attachFullFrameResult(frames[i].get(), blob, model_name, layer_name, frames.size(), i, blob_id);
                } else if (frames[i]->gva_base_inference->inference_region == ROI_LIST) {
                    attachROIResult(frames[i].get(), blob, model_name, layer_name, frames.size(), i);
                } else {
                    GST_WARNING("Not supported inference-region parameter value, inference results skipped.");
                }
            }
            ++blob_id;
        }
        return PostProcessor::ExitStatus::SUCCESS;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to extract inference results"));
    }
    return PostProcessor::ExitStatus::FAIL;
}
