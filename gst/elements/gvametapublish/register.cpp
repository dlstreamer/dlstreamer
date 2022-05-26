/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "file/gvametapublishfile.hpp"
#include "gvametapublish.hpp"

#include <gst/gst.h>

static gboolean plugin_init(GstPlugin *plugin) {
    gboolean result = TRUE;
    result &= gst_element_register(plugin, "gvametapublish", GST_RANK_NONE, GST_TYPE_GVA_META_PUBLISH);
    result &= gst_element_register(plugin, "gvametapublishfile", GST_RANK_NONE, GST_TYPE_GVA_META_PUBLISH_FILE);
    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvametapublish, PRODUCT_FULL_NAME " metapublish elements",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
