/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/transform.h"
#include "inference_openvino.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::TensorInferenceOpenVINODesc))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_openvino2,
                  PRODUCT_FULL_NAME " plugin for inference on OpenVINO™ toolkit backend", plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
