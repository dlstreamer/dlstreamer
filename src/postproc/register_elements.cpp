/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "detection_output.h"
#include "dlstreamer/gst/transform.h"
#include "yolo_v2.h"

#include "gva_tensor_meta.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::PostProcDetectionOutputDesc))
        return FALSE;
    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::PostProcYoloV2Desc))
        return FALSE;

    // register metadata
    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_postproc,
                  PRODUCT_FULL_NAME " elements for inference post-processing", plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
