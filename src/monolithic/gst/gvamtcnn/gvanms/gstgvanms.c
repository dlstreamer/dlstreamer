/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvanms.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#include "config.h"
#include "gva_caps.h"
#include "nms.h"
#include "utils.h"

#define ELEMENT_LONG_NAME "NMS(Non-Maximum Suppression) element for MTCNN"
#define ELEMENT_DESCRIPTION "NMS(Non-Maximum Suppression) element for MTCNN"

GST_DEBUG_CATEGORY_STATIC(gst_gva_nms_debug_category);
#define GST_CAT_DEFAULT gst_gva_nms_debug_category

#define DEFAULT_MIN_THRESHOLD 0
#define DEFAULT_MAX_THRESHOLD 100
#define DEFAULT_THRESHOLD 66

#define DEFAULT_MERGE FALSE

#define DEFAULT_MODE MODE_PNET

static void gst_gva_nms_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_nms_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_gva_nms_dispose(GObject *object);
static void gst_gva_nms_finalize(GObject *object);

static gboolean gst_gva_nms_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_gva_nms_start(GstBaseTransform *trans);
static gboolean gst_gva_nms_stop(GstBaseTransform *trans);
static gboolean gst_gva_nms_sink_event(GstBaseTransform *trans, GstEvent *event);
static GstFlowReturn gst_gva_nms_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_nms_cleanup(GstGvaNms *nms);
static void gst_gva_nms_reset(GstGvaNms *nms);
static GstStateChangeReturn gst_gva_nms_change_state(GstElement *element, GstStateChange transition);

enum {
    PROP_0,
    PROP_THRESHOLD,
    PROP_MODE,
    PROP_MERGE,
};

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaNms, gst_gva_nms, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_nms_debug_category, "gvanms", 0,
                                                "debug category for gvanms element"));

static void gst_gva_nms_class_init(GstGvaNmsClass *klass) {
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

    gobject_class->set_property = gst_gva_nms_set_property;
    gobject_class->get_property = gst_gva_nms_get_property;
    gobject_class->dispose = gst_gva_nms_dispose;
    gobject_class->finalize = gst_gva_nms_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_nms_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_nms_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_nms_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_nms_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_nms_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_nms_change_state);

    g_object_class_install_property(gobject_class, PROP_THRESHOLD,
                                    g_param_spec_uint("threshold", "NMS Threshold", "Non-maximum suppression threshold",
                                                      DEFAULT_MIN_THRESHOLD, DEFAULT_MAX_THRESHOLD, DEFAULT_THRESHOLD,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_MODE,
                                    g_param_spec_enum("mode", "Mode", "MTCNN mode", GST_TYPE_MTCNN_MODE, DEFAULT_MODE,
                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_MERGE,
                                    g_param_spec_boolean("merge", "Merge", "Merge candidates for final output",
                                                         DEFAULT_MERGE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_nms_cleanup(GstGvaNms *nms) {
    if (nms == NULL)
        return;

    GST_DEBUG_OBJECT(nms, "gst_gva_nms_cleanup");

    if (nms->info) {
        gst_video_info_free(nms->info);
        nms->info = NULL;
    }
}

static void gst_gva_nms_reset(GstGvaNms *nms) {
    GST_DEBUG_OBJECT(nms, "gst_gva_nms_reset");

    if (nms == NULL)
        return;

    gst_gva_nms_cleanup(nms);

    nms->threshold = DEFAULT_THRESHOLD;
    nms->mode = DEFAULT_MODE;
    nms->merge = DEFAULT_MERGE;
}

static GstStateChangeReturn gst_gva_nms_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaNms *nms;

    nms = GST_GVA_NMS(element);
    GST_DEBUG_OBJECT(nms, "gst_gva_nms_change_state");

    ret = GST_ELEMENT_CLASS(gst_gva_nms_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gst_gva_nms_reset(nms);
        break;
    }
    default:
        break;
    }

    return ret;
}

static void gst_gva_nms_init(GstGvaNms *nms) {
    GST_DEBUG_OBJECT(nms, "gst_gva_nms_init");
    GST_DEBUG_OBJECT(nms, "%s", GST_ELEMENT_NAME(GST_ELEMENT(nms)));

    gst_gva_nms_reset(nms);
}

static void gst_gva_nms_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaNms *nms = GST_GVA_NMS(object);

    GST_DEBUG_OBJECT(nms, "set_property");

    switch (prop_id) {
    case PROP_THRESHOLD:
        nms->threshold = g_value_get_uint(value);
        break;
    case PROP_MODE:
        nms->mode = g_value_get_enum(value);
        break;
    case PROP_MERGE:
        nms->merge = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_nms_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaNms *nms = GST_GVA_NMS(object);

    GST_DEBUG_OBJECT(nms, "get_property");

    switch (prop_id) {
    case PROP_THRESHOLD:
        g_value_set_uint(value, nms->threshold);
        break;
    case PROP_MODE:
        g_value_set_enum(value, nms->mode);
        break;
    case PROP_MERGE:
        g_value_set_boolean(value, nms->merge);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_nms_dispose(GObject *object) {
    GstGvaNms *nms = GST_GVA_NMS(object);

    GST_DEBUG_OBJECT(nms, "dispose");

    G_OBJECT_CLASS(gst_gva_nms_parent_class)->dispose(object);
}

static void gst_gva_nms_finalize(GObject *object) {
    GstGvaNms *nms = GST_GVA_NMS(object);

    GST_DEBUG_OBJECT(nms, "finalize");

    /* clean up object here */
    gst_gva_nms_cleanup(nms);

    G_OBJECT_CLASS(gst_gva_nms_parent_class)->finalize(object);
}

static gboolean gst_gva_nms_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaNms *nms = GST_GVA_NMS(trans);

    GST_DEBUG_OBJECT(nms, "set_caps");

    if (!nms->info) {
        nms->info = gst_video_info_new();
    }
    gst_video_info_from_caps(nms->info, incaps);

    return TRUE;
}

static gboolean gst_gva_nms_start(GstBaseTransform *trans) {
    GstGvaNms *nms = GST_GVA_NMS(trans);

    GST_DEBUG_OBJECT(nms, "start");

    GST_INFO_OBJECT(nms, "%s parameters:\n -- Mode: %s\n -- Threshold: %d\n -- Merge: %s\n",
                    GST_ELEMENT_NAME(GST_ELEMENT_CAST(nms)), mode_type_to_string(nms->mode), nms->threshold,
                    nms->merge ? "true" : "false");

    return TRUE;
}

static gboolean gst_gva_nms_stop(GstBaseTransform *trans) {
    GstGvaNms *nms = GST_GVA_NMS(trans);

    GST_DEBUG_OBJECT(nms, "stop");

    return TRUE;
}

/* sink and src pad event handlers */
static gboolean gst_gva_nms_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaNms *nms = GST_GVA_NMS(trans);

    GST_DEBUG_OBJECT(nms, "sink_event");

    return GST_BASE_TRANSFORM_CLASS(gst_gva_nms_parent_class)->sink_event(trans, event);
}

static GstFlowReturn gst_gva_nms_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaNms *nms = GST_GVA_NMS(trans);

    GST_DEBUG_OBJECT(nms, "transform_ip");

    return non_max_suppression(nms, buf) ? GST_FLOW_OK : GST_FLOW_ERROR;
}
