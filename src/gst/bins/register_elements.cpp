/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "meta_overlay.h"
#include "object_classify.h"
#include "object_detect.h"
#include "object_track.h"
#include "processbin.h"
#include "video_inference.h"

static gboolean plugin_init(GstPlugin *plugin) {
    gboolean result = TRUE;

    result &= gst_element_register(plugin, "processbin", GST_RANK_NONE, processbin_get_type());

    result &= gst_element_register(plugin, "video_inference", GST_RANK_NONE, video_inference_get_type());
    result &= gst_element_register(plugin, "object_detect", GST_RANK_NONE, object_detect_get_type());
    result &= gst_element_register(plugin, "object_classify", GST_RANK_NONE, object_classify_get_type());
    result &= gst_element_register(plugin, "object_track", GST_RANK_NONE, object_track_get_type());
    result &= gst_element_register(plugin, "meta_overlay", GST_RANK_NONE, meta_overlay_bin_get_type());

    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_bins,
                  PRODUCT_FULL_NAME " bin elements (implemented as GstBin)", plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
