/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"

#include "gstgvadetect.h"
#include "gva_caps.h"
#include "post_processors.h"

#define ELEMENT_LONG_NAME "Object detection (generates GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

enum {
    PROP_0,
    PROP_THRESHOLD,
};

#define DEFALUT_MIN_THRESHOLD 0.
#define DEFALUT_MAX_THRESHOLD 1.
#define DEFALUT_THRESHOLD 0.5

GST_DEBUG_CATEGORY_STATIC(gst_gva_detect_debug_category);
#define GST_CAT_DEFAULT gst_gva_detect_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaDetect, gst_gva_detect, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_detect_debug_category, "gvadetect", 0,
                                                "debug category for gvadetect element"));

void gst_gva_detect_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaDetect *gvadetect = (GstGvaDetect *)(object);

    GST_DEBUG_OBJECT(gvadetect, "set_property");

    switch (property_id) {
    case PROP_THRESHOLD:
        gvadetect->threshold = g_value_get_float(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_detect_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaDetect *gvadetect = (GstGvaDetect *)(object);

    GST_DEBUG_OBJECT(gvadetect, "get_property");

    switch (property_id) {
    case PROP_THRESHOLD:
        g_value_set_float(value, gvadetect->threshold);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_detect_class_init(GstGvaDetectClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = gst_gva_detect_set_property;
    gobject_class->get_property = gst_gva_detect_get_property;
    g_object_class_install_property(gobject_class, PROP_THRESHOLD,
                                    g_param_spec_float("threshold", "Threshold", "Threshold for inference",
                                                       DEFALUT_MIN_THRESHOLD, DEFALUT_MAX_THRESHOLD, DEFALUT_THRESHOLD,
                                                       (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

void gst_gva_detect_init(GstGvaDetect *gvadetect) {
    GST_DEBUG_OBJECT(gvadetect, "gst_gva_detect_init");
    GST_DEBUG_OBJECT(gvadetect, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvadetect)));

    gvadetect->threshold = DEFALUT_THRESHOLD;
    gvadetect->base_inference.post_proc = EXTRACT_DETECTION_RESULTS;
}
