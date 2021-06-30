/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include <gst/gst.h>

#include "gvaactionrecognitionbin.hpp"
#include "gvatensoracc.hpp"
#include "gvatensorinference.hpp"
#include "gvatensortometa.hpp"
#include "gvavideototensor.hpp"

#include "gva_custom_meta.hpp"

static gboolean plugin_init(GstPlugin *plugin) {
    GST_DEBUG_CATEGORY_INIT(gst_gva_action_recognition_bin_debug_category, "gvaactionrecognitionbin", 0,
                            "Debug category of gvaactionrecognitionbin");
    GST_DEBUG_CATEGORY_INIT(gst_gva_tensor_acc_debug_category, "gvatensoracc_debug", 0,
                            "Debug category of gvatensoracc");
    GST_DEBUG_CATEGORY_INIT(gst_gva_tensor_inference_debug_category, "gvatensorinference_debug", 0,
                            "Debug category of gvatensorinference");
    GST_DEBUG_CATEGORY_INIT(gst_gva_tensor_to_meta_debug_category, "gvatensortometa_debug", 0,
                            "Debug category of gvatensortometa");
    GST_DEBUG_CATEGORY_INIT(gst_gva_video_to_tensor_debug_category, "gvavideototensor_debug", 0,
                            "Debug category of gvavideototensor");

    gboolean result = TRUE;

    result &=
        gst_element_register(plugin, "gvaactionrecognitionbin", GST_RANK_NONE, GST_TYPE_GVA_ACTION_RECOGNITION_BIN);
    result &= gst_element_register(plugin, "gvatensoracc", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_ACC);
    result &= gst_element_register(plugin, "gvatensorinference", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_INFERENCE);
    result &= gst_element_register(plugin, "gvatensortometa", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_TO_META);
    result &= gst_element_register(plugin, "gvavideototensor", GST_RANK_NONE, GST_TYPE_GVA_VIDEO_TO_TENSOR);

    gst_gva_custom_meta_get_info();
    gst_gva_custom_meta_api_get_type();

    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, videoanalytics_preview, "Video Analytics preview elements",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
