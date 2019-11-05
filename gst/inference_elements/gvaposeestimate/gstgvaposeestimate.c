/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"

#include "gstgvaposeestimate.h"
#include "gva_caps.h"
#include "post_processors.h"

#define ELEMENT_LONG_NAME "Human Pose Estimation"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

enum {
    PROP_0,
};


GST_DEBUG_CATEGORY_STATIC(gst_gva_poseestimate_debug_category);
#define GST_CAT_DEFAULT gst_gva_poseestimate_debug_category

G_DEFINE_TYPE_WITH_CODE(GstGvaPoseestimate, gst_gva_poseestimate, GST_TYPE_GVA_BASE_INFERENCE,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_poseestimate_debug_category, "gvaposeestimate", 0,
                                                "debug category for gvaposeestimate element"));


void gst_gva_poseestimate_class_init(GstGvaPoseestimateClass *klass) {
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
}

void gst_gva_poseestimate_init(GstGvaPoseestimate *gvaposeestimate) {
    GST_DEBUG_OBJECT(gvaposeestimate, "gst_gva_poseestimate_init");
    GST_DEBUG_OBJECT(gvaposeestimate, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvaposeestimate)));

    gvaposeestimate->base_inference.post_proc = EXTRACT_POSEESTIMATION_RESULTS;
}
