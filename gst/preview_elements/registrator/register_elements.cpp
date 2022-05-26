/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include <gst/gst.h>

#include <gvaactionrecognitionbin.hpp>
#include <gvaclassifybin.hpp>
#include <gvadetectbin.hpp>
#include <gvadrop.hpp>
#include <gvafilter.hpp>
#include <gvahistory.hpp>
#include <gvainferencebin.hpp>
#include <gvatensoracc.hpp>
#include <gvatensorconverter.hpp>
#include <gvatensortometa.hpp>
#include <gvavideototensor.hpp>

#include "inference_backend/logger.h"
#include "logger_functions.h"

#include <gva_roi_ref_meta.hpp>

static gboolean plugin_init(GstPlugin *plugin) {
    set_log_function(GST_logger);
    GST_DEBUG_CATEGORY_INIT(gva_action_recognition_bin_debug_category, "gvaactionrecognitionbin", 0,
                            "Debug category of gvaactionrecognitionbin");
    GST_DEBUG_CATEGORY_INIT(gva_filter_debug_category, "gvafilter_debug", 0, "Debug category of gvafilter");
    GST_DEBUG_CATEGORY_INIT(gva_history_debug_category, "gvahistory_debug", 0, "Debug category of gvahistory");
    GST_DEBUG_CATEGORY_INIT(gva_tensor_acc_debug_category, "gvatensoracc_debug", 0, "Debug category of gvatensoracc");
    GST_DEBUG_CATEGORY_INIT(gva_tensor_to_meta_debug_category, "gvatensortometa_debug", 0,
                            "Debug category of gvatensortometa");
    GST_DEBUG_CATEGORY_INIT(gva_video_to_tensor_debug_category, "gvavideototensor_debug", 0,
                            "Debug category of gvavideototensor");
    GST_DEBUG_CATEGORY_INIT(gva_drop_debug_category, "gvadrop_debug", 0, "Debug category of gvadrop");

    gboolean result = TRUE;

    result &=
        gst_element_register(plugin, "gvaactionrecognitionbin", GST_RANK_NONE, GST_TYPE_GVA_ACTION_RECOGNITION_BIN);
    result &= gst_element_register(plugin, "gvainference", GST_RANK_NONE, GST_TYPE_GVA_INFERENCE_BIN);
    result &= gst_element_register(plugin, "gvaclassify", GST_RANK_NONE, GST_TYPE_GVA_CLASSIFY_BIN);
    result &= gst_element_register(plugin, "gvadetect", GST_RANK_NONE, GST_TYPE_GVA_DETECT_BIN);
    result &= gst_element_register(plugin, "gvafilter", GST_RANK_NONE, GST_TYPE_GVA_FILTER);
    result &= gst_element_register(plugin, "gvahistory", GST_RANK_NONE, GST_TYPE_GVA_HISTORY);
    result &= gst_element_register(plugin, "gvatensoracc", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_ACC);
    result &= gst_element_register(plugin, "gvatensorconverter", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_CONVERTER);
    result &= gst_element_register(plugin, "gvatensortometa", GST_RANK_NONE, GST_TYPE_GVA_TENSOR_TO_META);
    result &= gst_element_register(plugin, "gvavideototensor", GST_RANK_NONE, GST_TYPE_GVA_VIDEO_TO_TENSOR);
    result &= gst_element_register(plugin, "gvadrop", GST_RANK_NONE, GST_TYPE_GVA_DROP);

    // register metadata
    gva_roi_ref_meta_get_info();
    gva_roi_ref_meta_api_get_type();

    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return result;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, videoanalytics_preview, PRODUCT_FULL_NAME " preview elements",
                  plugin_init, PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
