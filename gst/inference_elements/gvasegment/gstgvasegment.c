/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "gstgvasegment.h"
#include "gva_caps.h"

#include "segmentation_post_processors_c.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#define UNUSED(x) (void)x

#define ELEMENT_LONG_NAME "Object segmentation (generates GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

GST_DEBUG_CATEGORY_STATIC(gst_gva_segment_debug_category);
#define GST_CAT_DEFAULT gst_gva_segment_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaSegment, gst_gva_segment, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_segment_debug_category, "gvasegment", 0,
                                                "debug category for gvasegment element"));

static void gst_gva_segment_finilize(GObject *object);
static void on_base_inference_initialized(GvaBaseInference *base_inference);
void gst_gva_segment_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
void gst_gva_segment_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
void gst_gva_segment_class_init(GstGvaSegmentClass *klass);

void gst_gva_segment_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    UNUSED(value);

    GstGvaSegment *gvasegment = (GstGvaSegment *)(object);

    GST_DEBUG_OBJECT(gvasegment, "set_property");

    // ask about preperties
    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_segment_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    UNUSED(value);

    GstGvaSegment *gvasegment = (GstGvaSegment *)(object);

    GST_DEBUG_OBJECT(gvasegment, "get_property");

    switch (property_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_segment_class_init(GstGvaSegmentClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gst_gva_segment_finilize;
    gobject_class->set_property = gst_gva_segment_set_property;
    gobject_class->get_property = gst_gva_segment_get_property;

    GvaBaseInferenceClass *base_inference_class = GVA_BASE_INFERENCE_CLASS(klass);
    base_inference_class->on_initialized = on_base_inference_initialized;
}

void gst_gva_segment_init(GstGvaSegment *gvasegment) {
    GST_DEBUG_OBJECT(gvasegment, "gst_gva_segment_init");
    GST_DEBUG_OBJECT(gvasegment, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvasegment)));
    GvaBaseInference *parent = (GvaBaseInference *)gvasegment;
    parent->inference_region = FULL_FRAME;
}

void gst_gva_segment_finilize(GObject *object) {
    GstGvaSegment *gvasegment = GST_GVA_SEGMENT(object);

    GST_DEBUG_OBJECT(gvasegment, "finalize");

    releaseSegmentationPostProcessor(gvasegment->base_inference.post_proc);
    gvasegment->base_inference.post_proc = NULL;

    G_OBJECT_CLASS(gst_gva_segment_parent_class)->finalize(object);
}

void on_base_inference_initialized(GvaBaseInference *base_inference) {
    GstGvaSegment *gvasegment = GST_GVA_SEGMENT(base_inference);
    GST_DEBUG_OBJECT(gvasegment, "on_base_inference_initialized");

    base_inference->post_proc = createSegmentationPostProcessor(base_inference->inference);
}
