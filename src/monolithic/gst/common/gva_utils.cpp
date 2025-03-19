/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
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

gboolean get_od_id(GstAnalyticsODMtd od_mtd, int *id) {
    GstAnalyticsTrackingMtd trk_mtd;
    if (gst_analytics_relation_meta_get_direct_related(od_mtd.meta, od_mtd.id, GST_ANALYTICS_REL_TYPE_ANY,
                                                       gst_analytics_tracking_mtd_get_mtd_type(), nullptr, &trk_mtd)) {
        guint64 tracking_id;
        GstClockTime tracking_first_seen, tracking_last_seen;
        gboolean tracking_lost;
        if (!gst_analytics_tracking_mtd_get_info(&trk_mtd, &tracking_id, &tracking_first_seen, &tracking_last_seen,
                                                 &tracking_lost)) {
            throw std::runtime_error("Failed to get tracking mtd info");
        }

        *id = tracking_id;
        return true;
    }
    return false;
}

void set_od_id(GstAnalyticsODMtd od_mtd, gint id) {
    gpointer state = nullptr;
    GstAnalyticsTrackingMtd trk_mtd;
    while (gst_analytics_relation_meta_get_direct_related(od_mtd.meta, od_mtd.id, GST_ANALYTICS_REL_TYPE_ANY,
                                                          gst_analytics_tracking_mtd_get_mtd_type(), &state,
                                                          &trk_mtd)) {
        if (!gst_analytics_relation_meta_set_relation(od_mtd.meta, GST_ANALYTICS_REL_TYPE_NONE, od_mtd.id,
                                                      trk_mtd.id)) {
            throw std::runtime_error("Failed to remove relation between od meta and tracking meta");
        }
    }

    if (!gst_analytics_relation_meta_add_tracking_mtd(od_mtd.meta, id, 0, &trk_mtd)) {
        throw std::runtime_error("Failed to add tracking metadata");
    }

    if (!gst_analytics_relation_meta_set_relation(od_mtd.meta, GST_ANALYTICS_REL_TYPE_RELATE_TO, od_mtd.id,
                                                  trk_mtd.id)) {
        throw std::runtime_error("Failed to set relation between od meta and tracking meta");
    }
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
