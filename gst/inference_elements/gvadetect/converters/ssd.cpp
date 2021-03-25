/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
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

                gfloat bbox_x = data[i * object_size + 3];
                gfloat bbox_y = data[i * object_size + 4];
                gfloat bbox_w = data[i * object_size + 5] - bbox_x;
                gfloat bbox_h = data[i * object_size + 6] - bbox_y;

                // This post processing happens not in main gstreamer thread (but in separate one). Thus, we can run
                // into a problem where buffer's gva_base_inference is already cleaned up (all frames are pushed
                // from decoder), so we can't use gva_base_inference's GstVideoInfo to get width and height. In this
                // case we use w and h of InferenceFrame to make VideoFrame box adding logic work correctly
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
                    bbox_x = bbox_x + bbox_w / 2 * (1 - roi_scale);
                    bbox_y = bbox_y + bbox_h / 2 * (1 - roi_scale);
                    bbox_w = bbox_w * roi_scale;
                    bbox_h = bbox_h * roi_scale;
                }
                addRoi(frames[image_id], bbox_x, bbox_y, bbox_w, bbox_h, label_id, confidence,
                       gst_structure_copy(detection_result),
                       labels); // each ROI gets its own copy, which is then
                                // owned by GstVideoRegionOfInterestMeta
            }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SSD post-processing"));
    }
    return true;
}
