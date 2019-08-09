/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"

#include "gstgvainference.h"
#include "gva_caps.h"
#include "post_processors.h"

#define ELEMENT_LONG_NAME "Generic full-frame inference (generates GstGVATensorMeta)"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

GST_DEBUG_CATEGORY_STATIC(gst_gva_inference_debug_category);
#define GST_CAT_DEFAULT gst_gva_inference_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaInference, gst_gva_inference, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_inference_debug_category, "gvainference", 0,
                                                "debug category for gvainference element"));

void gst_gva_inference_class_init(GstGvaInferenceClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

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

    gvainference->base_inference.post_proc = EXTRACT_INFERENCE_RESULTS;
}
