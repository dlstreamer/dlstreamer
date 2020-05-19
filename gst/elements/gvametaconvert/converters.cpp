/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <stdio.h>

#include "converters.h"
#include "gstgvametaconvert.h"
#include "gva_json_meta.h"
#include "gva_tensor_meta.h"
#include "video_frame.h"

#include "gva_utils.h"

#define UNUSED(x) (void)(x)

gboolean dump_detection(GstGvaMetaConvert *converter, GstBuffer *buffer) {
    if (converter == nullptr) {
        GST_ERROR_OBJECT(converter, "GVA meta convert data pointer is null");
        return FALSE;
    }

    try {
        GVA::VideoFrame video_frame(buffer, converter->info);
        for (GVA::RegionOfInterest &roi : video_frame.regions()) {
            auto rect = roi.rect();
            GST_INFO_OBJECT(converter,
                            "Detection: "
                            "id: %d, x: %d, y: %d, w: %d, h: %d, roi_type: %s",
                            roi.object_id(), rect.x, rect.y, rect.w, rect.h, roi.label().c_str());
        }
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(converter, "%s", e.what());
        return FALSE;
    }

    return TRUE;
}

GHashTable *get_converters() {
    GHashTable *converters = g_hash_table_new(g_direct_hash, g_direct_equal);

    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_JSON), (gpointer)to_json);
    g_hash_table_insert(converters, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_DETECTION), (gpointer)dump_detection);

    return converters;
}
