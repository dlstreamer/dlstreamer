/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "object_track.h"
#include "splitjoinbin.h"
#include "video_classify.h"
#include "video_detect.h"
#include "video_inference.h"

static gboolean plugin_init(GstPlugin *plugin) {
    gboolean result = TRUE;

    result &= gst_element_register(plugin, "splitjoinbin", GST_RANK_NONE, splitjoinbin_get_type());

    result &= gst_element_register(plugin, "video_inference", GST_RANK_NONE, video_inference_get_type());
    result &= gst_element_register(plugin, "video_detect", GST_RANK_NONE, video_detect_get_type());
    result &= gst_element_register(plugin, "video_classify", GST_RANK_NONE, video_classify_get_type());
    result &= gst_element_register(plugin, "object_track", GST_RANK_NONE, object_track_get_type());

    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_bins,
                  PRODUCT_FULL_NAME " bin elements (implemented as GstBin)", plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
