/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"
#include <gst/gst.h>

#include "gstgvaattachroi.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvaattachroi", GST_RANK_NONE, GST_TYPE_GVA_ATTACH_ROI))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvaattachroi, PRODUCT_FULL_NAME " elements", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
