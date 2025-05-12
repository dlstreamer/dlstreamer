/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"
#include <gst/gst.h>

#include "gstgvabboxregression.h"
#include "gstgvanms.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvabboxregression", GST_RANK_NONE, GST_TYPE_GVA_BBOX_REGRESSION))
        return FALSE;
    if (!gst_element_register(plugin, "gvanms", GST_RANK_NONE, GST_TYPE_GVA_NMS))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, gvamtcnn, PRODUCT_FULL_NAME " elements", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
