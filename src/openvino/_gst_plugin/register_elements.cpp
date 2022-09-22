/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/plugin.h"

static gboolean plugin_init(GstPlugin *plugin) {
    return register_elements_gst_plugin(dlstreamer_elements, plugin);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_openvino,
                  PRODUCT_FULL_NAME " plugin for inference on OpenVINO™ toolkit backend", plugin_init, PLUGIN_VERSION,
                  PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
