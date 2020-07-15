/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "converters/ssd.h"

#include "gstgvadetect.h"
#include "inference_backend/logger.h"
#include "video_frame.h"

using namespace DetectionPlugin;
using namespace Converters;

bool SSDConverter::process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                           const std::vector<std::shared_ptr<InferenceFrame>> &frames, GstStructure *detection_result,
                           double confidence_threshold, GValueArray *labels) {
    ITT_TASK(__FUNCTION__);
    bool flag = false;
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
                gint image_id = (gint)data[i * object_size + 0];
                gint label_id = (gint)data[i * object_size + 1];
                gdouble confidence = data[i * object_size + 2];
                gdouble x_min = data[i * object_size + 3];
                gdouble y_min = data[i * object_size + 4];
                gdouble x_max = data[i * object_size + 5];
                gdouble y_max = data[i * object_size + 6];

                // check image_id
                if (image_id < 0 || (size_t)image_id >= frames.size()) {
                    break;
                }

                // check confidence
                if (confidence < confidence_threshold) {
                    continue;
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
                GVA::VideoFrame video_frame(frames[image_id]->buffer, frames[image_id]->info);

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

                addRoi(frames[image_id]->buffer, frames[image_id]->info, x_min, y_min, x_max - x_min, y_max - y_min,
                       label_id, confidence, gst_structure_copy(detection_result),
                       labels); // each ROI gets its own copy, which is then
                                // owned by GstVideoRegionOfInterestMeta
            }
        }
        flag = true;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to do SSD post-processing"));
    }
    return flag;
}
