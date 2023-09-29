/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaaudiodetect.h"
#include <gst/gst.h>

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvaaudiodetect", GST_RANK_NONE, GST_TYPE_GVA_AUDIO_DETECT))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, audioanalytics, "Audio Analytics elements", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
