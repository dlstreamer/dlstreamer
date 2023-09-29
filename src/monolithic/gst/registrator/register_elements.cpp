/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include <gst/gst.h>

#include "gstgvaclassify.h"
#include "gstgvadetect.h"
#include "gstgvainference.h"
#include "gstgvametaaggregate.h"
#include "gstgvametaconvert.h"
#include "gstgvatrack.h"
#include "gstgvawatermarkimpl.h"
#include "gvawatermark.h"
#include "inference_backend/logger.h"
#include "logger_functions.h"

#include "gva_json_meta.h"
#include "gva_tensor_meta.h"

#include "runtime_feature_toggler.h"
#include <feature_toggling/ifeature_toggle.h>

#include "meta_overlay.h"
#include "object_classify.h"
#include "object_detect.h"
#include "object_track.h"
#include "video_inference.h"

CREATE_FEATURE_TOGGLE(UseMicroElements, "use-micro-elements",
                      "By default gvainference, gvadetect and gvaclassify use legacy elements. If you want to try new "
                      "micro elements approach set environment variable ENABLE_GVA_FEATURES=use-micro-elements.");

CREATE_FEATURE_TOGGLE(UseCPPElements, "use-cpp-elements", "Use elements implemented via C++ internal API");

static bool use_micro_elements() {
    // DLSTREAMER_GEN
    char *dlstreamer_gen = std::getenv("DLSTREAMER_GEN");
    bool use_gen2 = dlstreamer_gen && dlstreamer_gen[0] == '2';
    // ENABLE_GVA_FEATURES
    using namespace FeatureToggling::Runtime;
    RuntimeFeatureToggler toggler;
    toggler.configure(EnvironmentVariableOptionsReader().read("ENABLE_GVA_FEATURES"));
    bool use_cpp = toggler.enabled(UseCPPElements::id);
    bool use_micro = toggler.enabled(UseMicroElements::id);

    return use_gen2 || use_micro || use_cpp;
}

static gboolean plugin_init(GstPlugin *plugin) {
    set_log_function(GST_logger);

    // gvainference/gvadetect/gvaclassify/gvawatermark is bin on micro-elements or old implementation depending on env
    // variable
    if (use_micro_elements()) {
        if (!gst_element_register(plugin, "gvainference", GST_RANK_NONE, video_inference_get_type()))
            return FALSE;
        if (!gst_element_register(plugin, "gvadetect", GST_RANK_NONE, object_detect_get_type()))
            return FALSE;
        if (!gst_element_register(plugin, "gvaclassify", GST_RANK_NONE, object_classify_get_type()))
            return FALSE;
        if (!gst_element_register(plugin, "gvatrack", GST_RANK_NONE, GST_TYPE_GVA_TRACK)) // object_track_get_type()))
            return FALSE;
        if (!gst_element_register(plugin, "gvawatermark", GST_RANK_NONE,
                                  GST_TYPE_GVA_WATERMARK)) // meta_overlay_bin_get_type()))
            return FALSE;
    } else {
        if (!gst_element_register(plugin, "gvainference", GST_RANK_NONE, GST_TYPE_GVA_INFERENCE))
            return FALSE;
        if (!gst_element_register(plugin, "gvadetect", GST_RANK_NONE, GST_TYPE_GVA_DETECT))
            return FALSE;
        if (!gst_element_register(plugin, "gvaclassify", GST_RANK_NONE, GST_TYPE_GVA_CLASSIFY))
            return FALSE;
        if (!gst_element_register(plugin, "gvatrack", GST_RANK_NONE, GST_TYPE_GVA_TRACK))
            return FALSE;
        if (!gst_element_register(plugin, "gvawatermark", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK))
            return FALSE;
    }

    if (!gst_element_register(plugin, "gvametaconvert", GST_RANK_NONE, GST_TYPE_GVA_META_CONVERT))
        return FALSE;

    if (!gst_element_register(plugin, "gvawatermarkimpl", GST_RANK_NONE, GST_TYPE_GVA_WATERMARK_IMPL))
        return FALSE;

    if (!gst_element_register(plugin, "gvametaaggregate", GST_RANK_NONE, GST_TYPE_GVA_META_AGGREGATE))
        return FALSE;

    // register metadata
    gst_gva_json_meta_get_info();
    gst_gva_json_meta_api_get_type();
    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return TRUE;
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, videoanalytics, PRODUCT_FULL_NAME " elements", plugin_init,
                  PLUGIN_VERSION, PLUGIN_LICENSE, PACKAGE_NAME, GST_PACKAGE_ORIGIN)
