/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include <gst/gst.h>

#include "gstgvaclassify.h"
#include "gstgvadetect.h"
#include "gstgvafpscounter.h"
#include "gstgvaidentify.h"
#include "gstgvainference.h"
#include "gstgvametaconvert.h"
#include "gstgvametapublish.h"
#include "gstgvaskeleton.h"
#include "gstgvaspeedometer.h"
#include "gstgvawatermark.h"

static gboolean plugin_init(GstPlugin *plugin) {
    if (!gst_element_register(plugin, "gvainference", GST_RANK_NONE, GST_TYPE_GVA_INFERENCE))
        return FALSE;

    if (!gst_element_register(plugin, "gvadetect", GST_RANK_NONE, GST_TYPE_GVA_DETECT))
        return FALSE;

    if (!gst_element_register(plugin, "gvaclassify", GST_RANK_NONE, GST_TYPE_GVA_CLASSIFY))
        return FALSE;

    if (!gst_element_register(plugin, "gvaidentify", GST_RANK_NONE, GST_TYPE_GVA_IDENTIFY))
        return FALSE;

    if (!gst_element_register(plugin, "gvaskeleton", GST_RANK_NONE, GST_TYPE_GVA_SKELETON))
        return FALSE;

    if (!gst_element_register(plugin, "gvametaconvert", GST_RANK_NONE, GST_TYPE_GVA_META_CONVERT))
        return FALSE;

    if (!gst_element_register(plugin, "gvawatermark", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK))
        return FALSE;

    if (!gst_element_register(plugin, "gvafpscounter", GST_RANK_NONE, GST_TYPE_GVA_FPSCOUNTER))
        return FALSE;

    if (!gst_element_register(plugin, "gvaspeedometer", GST_RANK_NONE, GST_TYPE_GVA_SPEEDOMETER))
        return FALSE;

    if (!gst_element_register(plugin, "gvametapublish", GST_RANK_NONE, GST_TYPE_GVA_META_PUBLISH))
        return FALSE;

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, videoanalytics, "Video Analytics elements", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
