/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <stdio.h>

#include "converters.h"
#include "gstgvametaconvert.h"
#include "gva_json_meta.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "utils.h"
#include "video_frame.h"
#ifdef AUDIO
#include "audioconverter.h"
#endif

#define GST_CAT_DEFAULT gst_gva_meta_convert_debug_category

namespace {

gboolean dump_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    assert(converter && buffer && "Expected valid GstGvaMetaConvert and GstBuffer");

    try {
        if (converter->info) {
            GVA::VideoFrame video_frame(buffer, converter->info);
            for (GVA::RegionOfInterest &roi : video_frame.regions()) {
                auto rect = roi.rect();
                GST_INFO_OBJECT(converter,
                                "Detection: "
                                "id: %d, x: %d, y: %d, w: %d, h: %d, roi_type: %s",
                                roi.object_id(), rect.x, rect.y, rect.w, rect.h, roi.label().c_str());
            }
        }
#ifdef AUDIO
        else {
            dump_audio_detection(converter, buffer);
        }
#endif
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(converter, "%s", Utils::createNestedErrorMsg(e).c_str());
        return FALSE;
    }

    return TRUE;
}

} // namespace

GHashTable *get_converters() {
    GHashTable *converters = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_JSON), (gpointer)to_json);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_DETECTION), (gpointer)dump_detection);

    return converters;
}