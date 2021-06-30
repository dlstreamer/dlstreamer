/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_attacher.h"

#include "gva_base_inference.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "processor_types.h"

#include "inference_backend/safe_arithmetic.h"

#include <exception>
#include <memory>

using namespace post_processing;

MetaAttacher::Ptr MetaAttacher::create(int inference_type, int inference_region,
                                       const ModelImageInputInfo &input_image_info) {
    switch (inference_type) {
    case GST_GVA_DETECT_TYPE:
        return MetaAttacher::Ptr(new ROIToFrameAttacher(input_image_info));
    case GST_GVA_INFERENCE_TYPE:
    case GST_GVA_CLASSIFY_TYPE: {
        switch (inference_region) {
        case FULL_FRAME:
            return MetaAttacher::Ptr(new TensorToFrameAttacher(input_image_info));
        case ROI_LIST:
            return MetaAttacher::Ptr(new TensorToROIAttacher(input_image_info));

        default:
            throw std::runtime_error("Unknown inference region");
        }
    }

    default:
        throw std::runtime_error("Unknown inference type");
    }
}

void ROIToFrameAttacher::attach(const TensorsTable &tensors, InferenceFrames &frames) {
    if (frames.empty()) {
        throw std::invalid_argument("There are no inference frames");
    }

    if (frames.size() != tensors.size())
        throw std::logic_error("Size of the metadata array does not match the size of the inference frames: " +
                               std::to_string(tensors.size()) + " / " + std::to_string(frames.size()));

    for (size_t i = 0; i < frames.size(); ++i) {
        const auto &frame = frames[i];
        const auto &tensor = tensors[i];

        for (size_t j = 0; j < tensor.size(); ++j) {
            GstStructure *detection_tensor = tensor[j];

            uint32_t x_abs, y_abs, w_abs, h_abs;
            gst_structure_get_uint(detection_tensor, "x_abs", &x_abs);
            gst_structure_get_uint(detection_tensor, "y_abs", &y_abs);
            gst_structure_get_uint(detection_tensor, "w_abs", &w_abs);
            gst_structure_get_uint(detection_tensor, "h_abs", &h_abs);

            const gchar *label = gst_structure_get_string(detection_tensor, "label");

            GstBuffer **writable_buffer = &frame->buffer;
            gva_buffer_check_and_make_writable(writable_buffer, __PRETTY_FUNCTION__);
            GstVideoRegionOfInterestMeta *roi_meta =
                gst_buffer_add_video_region_of_interest_meta(*writable_buffer, label, x_abs, y_abs, w_abs, h_abs);

            gst_structure_remove_field(detection_tensor, "label");
            gst_structure_remove_field(detection_tensor, "x_abs");
            gst_structure_remove_field(detection_tensor, "y_abs");
            gst_structure_remove_field(detection_tensor, "w_abs");
            gst_structure_remove_field(detection_tensor, "h_abs");

            if (not roi_meta)
                throw std::runtime_error("Failed to add GstVideoRegionOfInterestMeta to buffer");

            gst_video_region_of_interest_meta_add_param(roi_meta, detection_tensor);
        }
    }
}

void TensorToFrameAttacher::attach(const TensorsTable &tensors_batch, InferenceFrames &frames) {
    if (frames.empty()) {
        throw std::invalid_argument("There are no inference frames");
    }

    if (frames.size() != tensors_batch.size())
        throw std::logic_error("Size of the metadata array does not match the size of the inference frames: " +
                               std::to_string(tensors_batch.size()) + " / " + std::to_string(frames.size()));

    for (size_t i = 0; i < frames.size(); ++i) {
        GstBuffer **writable_buffer = &frames[i]->buffer;

        for (GstStructure *tensor_data : tensors_batch[i]) {
            gva_buffer_check_and_make_writable(writable_buffer, __PRETTY_FUNCTION__);
            GstGVATensorMeta *tensor = GST_GVA_TENSOR_META_ADD(*writable_buffer);
            /* Tensor Meta already creates GstStructure during initialization */
            /* TODO: reduce amount of GstStructures copy from loading model-proc till attaching meta */
            if (tensor->data) {
                gst_structure_free(tensor->data);
            }
            tensor->data = tensor_data;
            gst_structure_set(tensor->data, "element_id", G_TYPE_STRING,
                              frames[i]->gva_base_inference->model_instance_id, NULL);
        }
    }
}

void TensorToROIAttacher::attach(const TensorsTable &tensors_batch, InferenceFrames &frames) {
    if (frames.empty()) {
        throw std::invalid_argument("There are no inference frames.");
    }

    if (frames.size() != tensors_batch.size())
        throw std::logic_error("Size of the metadata array does not match the size of the inference frames: " +
                               std::to_string(tensors_batch.size()) + " / " + std::to_string(frames.size()));

    for (size_t i = 0; i < frames.size(); ++i) {
        GstBuffer *buffer = frames[i]->buffer;

        GstVideoRegionOfInterestMeta *roi_meta = findROIMeta(buffer, frames[i]->roi);
        if (!roi_meta) {
            GST_WARNING("No detection tensors were found for this buffer in case of roi-list inference.");
            continue;
        }

        for (GstStructure *tensor_data : tensors_batch[i]) {
            gst_video_region_of_interest_meta_add_param(roi_meta, tensor_data);
            frames[i]->roi_classifications.push_back(tensor_data);
        }
    }
}

GstVideoRegionOfInterestMeta *TensorToROIAttacher::findROIMeta(GstBuffer *buffer,
                                                               const GstVideoRegionOfInterestMeta &frame_roi) {
    GstVideoRegionOfInterestMeta *meta = nullptr;
    gpointer state = nullptr;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, &frame_roi)) {
            return meta;
        }
    }
    return meta;
}
