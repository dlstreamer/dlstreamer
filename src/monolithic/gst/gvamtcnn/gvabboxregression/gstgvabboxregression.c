/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_caps.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include "bbox_regression.h"
#include "config.h"
#include "utils.h"

#define ELEMENT_LONG_NAME "Bounding-box regression element for MTCNN"
#define ELEMENT_DESCRIPTION "Bounding-box regression element for MTCNN"

#define DEFAULT_MODE MODE_PNET

GST_DEBUG_CATEGORY_STATIC(gst_gva_bbox_regression_debug_category);
#define GST_CAT_DEFAULT gst_gva_bbox_regression_debug_category

static void gst_gva_bbox_regression_set_property(GObject *object, guint prop_id, const GValue *value,
                                                 GParamSpec *pspec);
static void gst_gva_bbox_regression_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_gva_bbox_regression_dispose(GObject *object);
static void gst_gva_bbox_regression_finalize(GObject *object);

static gboolean gst_gva_bbox_regression_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_gva_bbox_regression_start(GstBaseTransform *trans);
static gboolean gst_gva_bbox_regression_stop(GstBaseTransform *trans);
static gboolean gst_gva_bbox_regression_sink_event(GstBaseTransform *trans, GstEvent *event);
static GstFlowReturn gst_gva_bbox_regression_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_bbox_regression_cleanup(GstGvaBBoxRegression *bboxregression);
static void gst_gva_bbox_regression_reset(GstGvaBBoxRegression *bboxregression);
static GstStateChangeReturn gst_gva_bbox_regression_change_state(GstElement *element, GstStateChange transition);

enum {
    PROP_0,
    PROP_MODE,
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaBBoxRegression, gst_gva_bbox_regression, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_bbox_regression_debug_category, "gvabboxregression", 0,
                                                "debug category for bboxregression element"));

static void gst_gva_bbox_regression_class_init(GstGvaBBoxRegressionClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_bbox_regression_set_property;
    gobject_class->get_property = gst_gva_bbox_regression_get_property;
    gobject_class->dispose = gst_gva_bbox_regression_dispose;
    gobject_class->finalize = gst_gva_bbox_regression_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_bbox_regression_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_bbox_regression_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_bbox_regression_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_bbox_regression_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_bbox_regression_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_bbox_regression_change_state);

    g_object_class_install_property(gobject_class, PROP_MODE,
                                    g_param_spec_enum("mode", "Mode", "MTCNN mode", GST_TYPE_MTCNN_MODE, DEFAULT_MODE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_bbox_regression_cleanup(GstGvaBBoxRegression *bboxregression) {
    if (bboxregression == NULL)
        return;

    GST_DEBUG_OBJECT(bboxregression, "gst_gva_bbox_regression_cleanup");

    if (bboxregression->info) {
        gst_video_info_free(bboxregression->info);
        bboxregression->info = NULL;
    }
}

static void gst_gva_bbox_regression_reset(GstGvaBBoxRegression *bboxregression) {
    GST_DEBUG_OBJECT(bboxregression, "gst_gva_bbox_regression_reset");

    if (bboxregression == NULL)
        return;

    gst_gva_bbox_regression_cleanup(bboxregression);

    bboxregression->mode = DEFAULT_MODE;
}

static GstStateChangeReturn gst_gva_bbox_regression_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaBBoxRegression *bboxregression;

    bboxregression = GST_GVA_BBOX_REGRESSION(element);
    GST_DEBUG_OBJECT(bboxregression, "gst_gva_bbox_regression_change_state");

    ret = GST_ELEMENT_CLASS(gst_gva_bbox_regression_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gst_gva_bbox_regression_reset(bboxregression);
        break;
    }
    default:
        break;
    }

    return ret;
}

static void gst_gva_bbox_regression_init(GstGvaBBoxRegression *bboxregression) {
    GST_DEBUG_OBJECT(bboxregression, "gst_gva_bbox_regression_init");
    GST_DEBUG_OBJECT(bboxregression, "%s", GST_ELEMENT_NAME(GST_ELEMENT(bboxregression)));

    gst_gva_bbox_regression_reset(bboxregression);
}

static void gst_gva_bbox_regression_set_property(GObject *object, guint prop_id, const GValue *value,
                                                 GParamSpec *pspec) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(object);

    GST_DEBUG_OBJECT(bboxregression, "set_property");

    switch (prop_id) {
    case PROP_MODE:
        bboxregression->mode = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_bbox_regression_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(object);

    GST_DEBUG_OBJECT(bboxregression, "get_property");

    switch (prop_id) {
    case PROP_MODE:
        g_value_set_enum(value, bboxregression->mode);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_bbox_regression_dispose(GObject *object) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(object);

    GST_DEBUG_OBJECT(bboxregression, "dispose");

    G_OBJECT_CLASS(gst_gva_bbox_regression_parent_class)->dispose(object);
}

static void gst_gva_bbox_regression_finalize(GObject *object) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(object);

    GST_DEBUG_OBJECT(bboxregression, "finalize");

    /* clean up object here */
    gst_gva_bbox_regression_cleanup(bboxregression);

    G_OBJECT_CLASS(gst_gva_bbox_regression_parent_class)->finalize(object);
}

static gboolean gst_gva_bbox_regression_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(trans);

    GST_DEBUG_OBJECT(bboxregression, "set_caps");

    if (!bboxregression->info) {
        bboxregression->info = gst_video_info_new();
    }
    gst_video_info_from_caps(bboxregression->info, incaps);

    return TRUE;
}

static gboolean gst_gva_bbox_regression_start(GstBaseTransform *trans) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(trans);

    GST_DEBUG_OBJECT(bboxregression, "start");

    GST_INFO_OBJECT(bboxregression, "%s parameters:\n -- Mode: %s\n",
                    GST_ELEMENT_NAME(GST_ELEMENT_CAST(bboxregression)), mode_type_to_string(bboxregression->mode));

    return TRUE;
}

static gboolean gst_gva_bbox_regression_stop(GstBaseTransform *trans) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(trans);

    GST_DEBUG_OBJECT(bboxregression, "stop");

    return TRUE;
}

/* sink and src pad event handlers */
static gboolean gst_gva_bbox_regression_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(trans);

    GST_DEBUG_OBJECT(bboxregression, "sink_event");

    return GST_BASE_TRANSFORM_CLASS(gst_gva_bbox_regression_parent_class)->sink_event(trans, event);
}

static GstFlowReturn gst_gva_bbox_regression_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaBBoxRegression *bboxregression = GST_GVA_BBOX_REGRESSION(trans);

    GST_DEBUG_OBJECT(bboxregression, "transform_ip");

    return bbox_regression(bboxregression, buf) ? GST_FLOW_OK : GST_FLOW_ERROR;
}
