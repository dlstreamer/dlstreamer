/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/ssd.h"

#include "gstgvadetect.h"
#include "gva_utils.h"
#include "inference_backend/logger.h"
#include "video_frame.h"

using namespace DetectionPlugin;
using namespace Converters;

/**
 * Compares to metas of type GstVideoRegionOfInterestMeta by roi_type and coordinates.
 *
 * @param[in] left - pointer to first GstVideoRegionOfInterestMeta operand.
 * @param[in] right - pointer to second GstVideoRegionOfInterestMeta operand.
 *
 * @return true if given metas are equal, false otherwise.
 */
inline bool sameRegion(GstVideoRegionOfInterestMeta *left, GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

/**
 * Iterating through GstBuffer's metas and searching for meta that matching frame's ROI.
 *
 * @param[in] frame - pointer to InferenceFrame containing pointers to buffer and ROI.
 *
 * @return GstVideoRegionOfInterestMeta - meta of GstBuffer, or nullptr.
 *
 * @throw std::invalid_argument when GstBuffer is nullptr.
 */
inline GstVideoRegionOfInterestMeta *findDetectionMeta(InferenceFrame *frame) {
    GstBuffer *buffer = frame->buffer;
    if (not buffer)
        throw std::invalid_argument("Inference frame's buffer is nullptr");
    auto frame_roi = &frame->roi;
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, frame_roi)) {
            return meta;
        }
    }
    return meta;
}

/**
 * Applies inference results to the buffer. Extracting data from each resulting blob,
 * adding ROI to the corresponding frame and addting metas to detection_result.
 *
 * @param[in] output_blobs - blobs containing inference results.
 * @param[in] frames - frames processed during inference.
 * @param[in] detection_result - detection tensor to attach meta in.
 * @param[in] confidence_threshold - value between 0 and 1 determining the accuracy of inference results to be handled.
 * @param[in] labels - GValueArray containing layers info from output_blobs.
 *
 * @return true if everything processed without exceptions.
 *
 * @throw std::invalid_argument when either blobs are invalid or their info is invalid.
 */
bool SSDConverter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                           const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                           double confidence_threshold, GValueArray *labels) {
    ITT_TASK(__FUNCTION__);
    try {
        if (not detection_result)
            throw std::invalid_argument("detection_result tensor is nullptr");
        gdouble roi_scale = 1.0;
        gst_structure_get_double(detection_result, "roi_scale", &roi_scale);

        // Check whether we can handle this blob instead iterator
        for (const auto &blob_iter : output_blobs) {
            InferenceBackend::OutputBlob::Ptr blob = blob_iter.second;
            if (not blob)
                throw std::invalid_argument("Output blob is nullptr");

            const float *data = (const float *)blob->GetData();
            if (not data)
                throw std::invalid_argument("Output blob data is nullptr");

            auto dims = blob->GetDims();
            guint dims_size = dims.size();

            static constexpr guint min_dims_size = 2;
            if (dims_size < min_dims_size)
                throw std::invalid_argument("Output blob dimentions size " + std::to_string(dims_size) +
                                            " is not supported (less than " + std::to_string(min_dims_size) + ")");

            for (guint i = min_dims_size + 1; i < dims_size; ++i) {
                if (dims[dims_size - i] != 1)
                    throw std::invalid_argument("All output blob dimensions, except for object size and max "
                                                "objects count, must be equal to 1");
            }

            guint object_size = dims[dims_size - 1];
            static constexpr guint supported_object_size = 7; // SSD DetectionOutput format
            if (object_size != supported_object_size)
                throw std::invalid_argument("Object size dimension of output blob is set to " +
                                            std::to_string(object_size) + ", but only " +
                                            std::to_string(supported_object_size) + " supported");

            GstVideoInfo video_info;
            guint max_proposal_count = dims[dims_size - 2];
            for (guint i = 0; i < max_proposal_count; ++i) {
                gint image_id = static_cast<gint>(data[i * object_size + 0]);
                /* check if 'image_id' contains a valid index for 'frames' vector */
                if (image_id < 0 || (size_t)image_id >= frames.size()) {
                    break;
                }

                gint label_id = static_cast<gint>(data[i * object_size + 1]);
                gdouble confidence = data[i * object_size + 2];
                /* discard inference results that do not match 'confidence_threshold' */
                if (confidence < confidence_threshold) {
                    continue;
                }

                gdouble x_min = data[i * object_size + 3];
                gdouble y_min = data[i * object_size + 4];
                gdouble x_max = data[i * object_size + 5];
                gdouble y_max = data[i * object_size + 6];
                /* In case of gvadetect with inference-region=roi-list we get coordinates relative to ROI.
                 * We need to convert them to coordinates relative to the full frame. */
                if (frames[image_id]->gva_base_inference->inference_region == ROI_LIST) {
                    GstVideoRegionOfInterestMeta *meta = findDetectionMeta(frames[image_id].get());
                    if (meta) {
                        x_min = (meta->x + meta->w * data[i * object_size + 3]) / frames[image_id]->info->width;
                        y_min = (meta->y + meta->h * data[i * object_size + 4]) / frames[image_id]->info->height;
                        x_max = (meta->x + meta->w * data[i * object_size + 5]) / frames[image_id]->info->width;
                        y_max = (meta->y + meta->h * data[i * object_size + 6]) / frames[image_id]->info->height;
                    }
                }

                // This post processing happens not in main gstreamer thread (but in separate one). Thus, we can run
                // into a problem where buffer's gva_base_inference is already cleaned up (all frames are pushed from
                // decoder), so we can't use gva_base_inference's GstVideoInfo to get width and height. In this case we
                // use w and h of InferenceFrame to make VideoFrame box adding logic work correctly
                if (frames[image_id]->gva_base_inference->info) {
                    video_info = *frames[image_id]->gva_base_inference->info;
                } else {
                    video_info.width = frames[image_id]->roi.w;
                    video_info.height = frames[image_id]->roi.h;
                }

                // TODO: in future we must return to use GVA::VideoFrame
                // GVA::VideoFrame video_frame(frames[image_id]->buffer, frames[image_id]->info);

                // TODO: check if we can simplify below code further
                // apply roi_scale if set
                if (roi_scale > 0 && roi_scale != 1) {
                    gdouble x_center = (x_max + x_min) * 0.5;
                    gdouble y_center = (y_max + y_min) * 0.5;
                    gdouble new_w = (x_max - x_min) * roi_scale;
                    gdouble new_h = (y_max - y_min) * roi_scale;
                    x_min = x_center - new_w * 0.5;
                    x_max = x_center + new_w * 0.5;
                    y_min = y_center - new_h * 0.5;
                    y_max = y_center + new_h * 0.5;
                }

                addRoi(frames[image_id]->buffer, frames[image_id]->info, frames[image_id]->image_transform_info, x_min,
                       y_min, x_max - x_min, y_max - y_min, label_id, confidence, gst_structure_copy(detection_result),
                       labels); // each ROI gets its own copy, which is then
                                // owned by GstVideoRegionOfInterestMeta
            }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SSD post-processing"));
    }
    return true;
}
