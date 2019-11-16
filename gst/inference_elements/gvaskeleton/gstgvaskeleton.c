/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaskeleton.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>


#define ELEMENT_LONG_NAME "Human Pose Estimation"
#define ELEMENT_DESCRIPTION "Human Pose Estimation"

GST_DEBUG_CATEGORY_STATIC(gst_gva_skeleton_debug_category);
#define GST_CAT_DEFAULT gst_gva_skeleton_debug_category

enum { PROP_0, PROP_MODEL_PATH };

static void gst_gva_skeleton_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_skeleton_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void gst_gva_skeleton_finalize(GObject *object);
static void gst_gva_skeleton_cleanup(GstGvaSkeleton *gvaskeleton);

static GstFlowReturn gst_gva_skeleton_transform_ip(GstBaseTransform *trans, GstBuffer *buf);

static gboolean gst_gva_skeleton_start(GstBaseTransform *base);

G_DEFINE_TYPE_WITH_CODE(GstGvaSkeleton, gst_gva_skeleton, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_skeleton_debug_category, "skeleton", 0,
                                                "debug category for skeleton element"));

void gst_gva_skeleton_class_init(GstGvaSkeletonClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);

    gobject_class->finalize = gst_gva_skeleton_finalize;
    gobject_class->set_property = gst_gva_skeleton_set_property;
    gobject_class->get_property = gst_gva_skeleton_get_property;
    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
    g_object_class_install_property(gobject_class, PROP_MODEL_PATH,
                                    g_param_spec_string("model path",         // name
                                                        "model path",         // nickname
                                                        "Path to model path", // description
                                                        "",   // default
                                                        G_PARAM_WRITABLE    // flags
                                                        ));
    

    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_skeleton_transform_ip);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_skeleton_start);
}

void gst_gva_skeleton_init(GstGvaSkeleton *skeleton) {
    GST_INFO_OBJECT(skeleton, "Initializing plugin");

}

void gst_gva_skeleton_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(skeleton, "set_property");

    switch (prop_id) {
    case PROP_MODEL_PATH:
        skeleton->model_path = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_skeleton_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(skeleton, "get_property");

    switch (prop_id) {
    case PROP_MODEL_PATH:
        g_value_set_string(value, skeleton->model_path);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_skeleton_cleanup(GstGvaSkeleton *skeleton) {
    if (skeleton == NULL)
        return;

    GST_DEBUG_OBJECT(skeleton, "gst_gva_skeleton_cleanup");

    g_free(skeleton->model_path);
    skeleton->model_path = NULL;

}

void gst_gva_skeleton_finalize(GObject *object) {
    GstGvaSkeleton *gvaskeleton = GST_GVA_SKELETON(object);

    GST_DEBUG_OBJECT(gvaskeleton, "finalize");

    /* clean up object here */

    G_OBJECT_CLASS(gst_gva_skeleton_parent_class)->finalize(object);
}

GstFlowReturn gst_gva_skeleton_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaSkeleton *gvaskeleton = GST_GVA_SKELETON(trans);

    GST_DEBUG_OBJECT(gvaskeleton, "transform_ip");
    if (!gst_pad_is_linked(GST_BASE_TRANSFORM_SRC_PAD(trans))) {
        return GST_BASE_TRANSFORM_FLOW_DROPPED;
    }


    return 0;

    /* return GST_FLOW_OK; FIXME shouldn't signal about dropping frames in inplace transform function*/
}

gboolean gst_gva_skeleton_start(GstBaseTransform *base) {
    GstGvaSkeleton *skeleton = GST_GVA_SKELETON(base);
    GST_INFO_OBJECT(skeleton, "Start");

    GST_INFO_OBJECT(skeleton, "Start is successfull");
    return TRUE;
}
