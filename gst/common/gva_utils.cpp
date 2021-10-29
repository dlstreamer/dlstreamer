/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_utils.h"

#include <inference_backend/image.h>
#include <inference_backend/logger.h>

#include <cassert>
#include <string>
#include <thread>

gboolean get_object_id(GstVideoRegionOfInterestMeta *meta, int *id) {
    GstStructure *object_id = gst_video_region_of_interest_meta_get_param(meta, "object_id");
    return object_id && gst_structure_get_int(object_id, "id", id);
}

void set_object_id(GstVideoRegionOfInterestMeta *meta, gint id) {
    GstStructure *object_id = gst_structure_new("object_id", "id", G_TYPE_INT, id, NULL);
    gst_video_region_of_interest_meta_add_param(meta, object_id);
}

void gva_buffer_check_and_make_writable(GstBuffer **buffer, const char *called_function_name) {
    assert(called_function_name);

    ITT_TASK(std::string(__FUNCTION__) + called_function_name);

    if (!(buffer and *buffer)) {
        GST_ERROR("%s: Buffer is null.", called_function_name);
        return;
    }

    if (!gst_buffer_is_writable(*buffer)) {
        GST_WARNING("%s: Buffer is not writable.", called_function_name);
        /* Waits for a bit to give a buffer time to become writable */
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    if (!gst_buffer_is_writable(*buffer)) {
        GST_WARNING("%s: Making a writable buffer requires buffer copy.", called_function_name);
        *buffer = gst_buffer_make_writable(*buffer);
    }
}

int gst_format_to_fourcc(int format) {
    switch (format) {
    case GST_VIDEO_FORMAT_NV12:
        GST_DEBUG("GST_VIDEO_FORMAT_NV12");
        return InferenceBackend::FourCC::FOURCC_NV12;
    case GST_VIDEO_FORMAT_BGR:
        GST_DEBUG("GST_VIDEO_FORMAT_BGR");
        return InferenceBackend::FourCC::FOURCC_BGR;
    case GST_VIDEO_FORMAT_BGRx:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRx");
        return InferenceBackend::FourCC::FOURCC_BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRA");
        return InferenceBackend::FourCC::FOURCC_BGRA;
    case GST_VIDEO_FORMAT_RGBA:
        GST_DEBUG("GST_VIDEO_FORMAT_RGBA");
        return InferenceBackend::FourCC::FOURCC_RGBA;
    case GST_VIDEO_FORMAT_I420:
        GST_DEBUG("GST_VIDEO_FORMAT_I420");
        return InferenceBackend::FourCC::FOURCC_I420;
    }

    GST_WARNING("Unsupported GST Format: %d.", format);
    return 0;
}
