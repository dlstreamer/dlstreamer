/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/transform.h"
#include "gva_tensor_meta.h"
#include "tensor_normalize_opencv.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::TensorNormalizeOpenCVDesc))
        return FALSE;

    // register metadata
    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_opencv,
                  PRODUCT_FULL_NAME " elements based on OpenCV", plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE,
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
