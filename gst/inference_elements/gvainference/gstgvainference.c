/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvainference.h"

#include "inference_post_processors_c.h"

#include "gva_caps.h"

#include "config.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#define ELEMENT_LONG_NAME "Generic full-frame inference (generates GstGVATensorMeta)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

GST_DEBUG_CATEGORY_STATIC(gst_gva_inference_debug_category);
#define GST_CAT_DEFAULT gst_gva_inference_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaInference, gst_gva_inference, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_inference_debug_category, "gvainference", 0,
                                                "debug category for gvainference element"));

static void gst_gva_inference_finilize(GObject *object);
static void on_base_inference_initialized(GvaBaseInference *base_inference);

void gst_gva_inference_class_init(GstGvaInferenceClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gst_gva_inference_finilize;

    GvaBaseInferenceClass *base_inference_class = GVA_BASE_INFERENCE_CLASS(klass);
    base_inference_class->on_initialized = on_base_inference_initialized;

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
}

void gst_gva_inference_init(GstGvaInference *gvainference) {
    GST_DEBUG_OBJECT(gvainference, "gst_gva_inference_init");
    GST_DEBUG_OBJECT(gvainference, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvainference)));
}

static void gst_gva_inference_finilize(GObject *object) {
    GstGvaInference *gva_inference = GST_GVA_INFERENCE(object);

    GST_DEBUG_OBJECT(gva_inference, "finalize");

    releaseInferencePostProcessor(gva_inference->base_inference.post_proc);
    gva_inference->base_inference.post_proc = NULL;

    G_OBJECT_CLASS(gst_gva_inference_parent_class)->finalize(object);
}

static void on_base_inference_initialized(GvaBaseInference *base_inference) {
    GstGvaInference *gvainference = GST_GVA_INFERENCE(base_inference);

    GST_DEBUG_OBJECT(gvainference, "on_base_inference_initialized");

    base_inference->post_proc = createInferencePostProcessor(base_inference->inference);
}
