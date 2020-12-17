/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_utils.h"

#include "inference_backend/logger.h"

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
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!gst_buffer_is_writable(*buffer)) {
        GST_WARNING("%s: Making a writable buffer requires buffer copy.", called_function_name);
        *buffer = gst_buffer_make_writable(*buffer);
    }
}
