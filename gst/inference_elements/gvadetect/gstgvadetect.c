/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "gstgvadetect.h"
#include "gva_caps.h"

#include "detection_post_processors_c.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#define ELEMENT_LONG_NAME "Object detection (generates GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION                                                                                            \
    "Performs object detection using SSD-like "                                                                        \
    "(including MobileNet-V1/V2 and ResNet), YoloV2/YoloV3/YoloV2-tiny/YoloV3-tiny "                                   \
    "and FasterRCNN-like object detection models."

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

static void gst_gva_detect_finilize(GObject *object);
static void on_base_inference_initialized(GvaBaseInference *base_inference);

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
    gobject_class->finalize = gst_gva_detect_finilize;
    gobject_class->set_property = gst_gva_detect_set_property;
    gobject_class->get_property = gst_gva_detect_get_property;

    GvaBaseInferenceClass *base_inference_class = GVA_BASE_INFERENCE_CLASS(klass);
    base_inference_class->on_initialized = on_base_inference_initialized;

    g_object_class_install_property(
        gobject_class, PROP_THRESHOLD,
        g_param_spec_float("threshold", "Threshold",
                           "Threshold for detection results. Only regions of interest "
                           "with confidence values above the threshold will be added to the frame",
                           DEFALUT_MIN_THRESHOLD, DEFALUT_MAX_THRESHOLD, DEFALUT_THRESHOLD,
                           (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

void gst_gva_detect_init(GstGvaDetect *gvadetect) {
    GST_DEBUG_OBJECT(gvadetect, "gst_gva_detect_init");
    GST_DEBUG_OBJECT(gvadetect, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvadetect)));

    gvadetect->base_inference.type = GST_GVA_DETECT_TYPE;
    gvadetect->threshold = DEFALUT_THRESHOLD;
}

void gst_gva_detect_finilize(GObject *object) {
    GstGvaDetect *gvadetect = GST_GVA_DETECT(object);

    GST_DEBUG_OBJECT(gvadetect, "finalize");

    releaseDetectionPostProcessor(gvadetect->base_inference.post_proc);
    gvadetect->base_inference.post_proc = NULL;

    G_OBJECT_CLASS(gst_gva_detect_parent_class)->finalize(object);
}

void on_base_inference_initialized(GvaBaseInference *base_inference) {
    GstGvaDetect *gvadetect = GST_GVA_DETECT(base_inference);

    GST_DEBUG_OBJECT(gvadetect, "on_base_inference_initialized");

    base_inference->post_proc = createDetectionPostProcessor(base_inference->inference);
}
