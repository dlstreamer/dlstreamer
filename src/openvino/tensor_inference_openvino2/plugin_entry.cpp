/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/gst/transform.h"
#include "inference_openvino2.h"

static gboolean plugin_init(GstPlugin *plugin) {
    return dlstreamer::register_transform_as_gstreamer(plugin, dlstreamer::TensorInferenceOpenVINO_2_0_Desc);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, dlstreamer_openvino_2_0_api,
                  PRODUCT_FULL_NAME " plugin for inference on OpenVINO™ toolkit "
                                    "backend using 2.0 API",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
