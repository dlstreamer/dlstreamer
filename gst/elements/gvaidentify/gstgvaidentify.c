/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvaidentify.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#include "config.h"

#define UNUSED(x) (void)(x)

#define ELEMENT_LONG_NAME                                                                                              \
    "Object/face recognition: match re-identification feature vector against registered feature vectors"
#define ELEMENT_DESCRIPTION                                                                                            \
    "Object/face recognition: match re-identification feature vector against registered feature vectors"

GST_DEBUG_CATEGORY_STATIC(gst_gva_identify_debug_category);
#define GST_CAT_DEFAULT gst_gva_identify_debug_category

#define DEFAULT_MODEL NULL
#define DEFAULT_GALLERY ""
#define DEFAULT_META_FORMAT ""

#define DEFAULT_MIN_THRESHOLD 0.
#define DEFAULT_MAX_THRESHOLD 1.
#define DEFAULT_THRESHOLD 0.7

#define DEFAULT_TRACKER FALSE

static void gst_gva_identify_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_identify_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_gva_identify_dispose(GObject *object);
static void gst_gva_identify_finalize(GObject *object);

static gboolean gst_gva_identify_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);

static gboolean gst_gva_identify_start(GstBaseTransform *trans);
static gboolean gst_gva_identify_stop(GstBaseTransform *trans);
static gboolean gst_gva_identify_sink_event(GstBaseTransform *trans, GstEvent *event);

static GstFlowReturn gst_gva_identify_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static void gst_gva_identify_cleanup(GstGvaIdentify *gvaidentify);
static void gst_gva_identify_reset(GstGvaIdentify *gvaidentify);
static GstStateChangeReturn gst_gva_identify_change_state(GstElement *element, GstStateChange transition);

enum { PROP_0, PROP_MODEL, PROP_GALLERY, PROP_THRESHOLD, PROP_TRACKER };

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#ifndef DISABLE_VAAPI
#define VA_SURFACE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:VASurface", "{ NV12 }") "; "
#else
#define VA_SURFACE_CAPS
#endif

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }")
#define INFERENCE_CAPS DMA_BUFFER_CAPS VA_SURFACE_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SINK_CAPS INFERENCE_CAPS
#define VIDEO_SRC_CAPS INFERENCE_CAPS

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstGvaIdentify, gst_gva_identify, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_identify_debug_category, "gvaidentify", 0,
                                                "debug category for gvaidentify element"));

static void gst_gva_identify_class_init(GstGvaIdentifyClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gobject_class->set_property = gst_gva_identify_set_property;
    gobject_class->get_property = gst_gva_identify_get_property;
    gobject_class->dispose = gst_gva_identify_dispose;
    gobject_class->finalize = gst_gva_identify_finalize;
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_identify_set_caps);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_identify_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_identify_stop);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_identify_sink_event);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_identify_transform_ip);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_identify_change_state);

    g_object_class_install_property(gobject_class, PROP_MODEL,
                                    g_param_spec_string("model", "Model", "Inference model file path", DEFAULT_MODEL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_GALLERY,
                                    g_param_spec_string("gallery", "Gallery",
                                                        "JSON file with list of image examples for each known "
                                                        "object/face/person. See samples for JSON format examples",
                                                        DEFAULT_GALLERY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_TRACKER,
                                    g_param_spec_boolean("tracker", "Tracker",
                                                         "Enable position-based object tracker. "
                                                         "The tracker assigns object_id to all tracked objects and "
                                                         "smooths identification results over period of time",
                                                         DEFAULT_TRACKER, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_THRESHOLD,
                                    g_param_spec_float("threshold", "Threshold",
                                                       "Identification threshold"
                                                       "for object comparison versus objects in the gallery",
                                                       DEFAULT_MIN_THRESHOLD, DEFAULT_MAX_THRESHOLD, DEFAULT_THRESHOLD,
                                                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_identify_cleanup(GstGvaIdentify *gvaidentify) {
    if (gvaidentify == NULL)
        return;

    GST_DEBUG_OBJECT(gvaidentify, "gst_gva_identify_cleanup");

    if (gvaidentify->identifier) {
        identifier_delete(gvaidentify->identifier);
        gvaidentify->identifier = NULL;
    }

    g_free(gvaidentify->model);
    gvaidentify->model = NULL;

    g_free(gvaidentify->gallery);
    gvaidentify->gallery = NULL;

    gvaidentify->initialized = FALSE;
}

static void gst_gva_identify_reset(GstGvaIdentify *gvaidentify) {
    GST_DEBUG_OBJECT(gvaidentify, "gst_gva_identify_reset");

    if (gvaidentify == NULL)
        return;

    gst_gva_identify_cleanup(gvaidentify);

    gvaidentify->model = g_strdup(DEFAULT_MODEL);
    gvaidentify->gallery = g_strdup(DEFAULT_GALLERY);
    gvaidentify->tracker = DEFAULT_TRACKER;
    gvaidentify->threshold = DEFAULT_THRESHOLD;
    gvaidentify->identifier = NULL;
    gvaidentify->initialized = FALSE;
}

static GstStateChangeReturn gst_gva_identify_change_state(GstElement *element, GstStateChange transition) {
    GstStateChangeReturn ret;
    GstGvaIdentify *gvaidentify;

    gvaidentify = GST_GVA_IDENTIFY(element);
    GST_DEBUG_OBJECT(gvaidentify, "gst_gva_identify_change_state");

    ret = GST_ELEMENT_CLASS(gst_gva_identify_parent_class)->change_state(element, transition);

    switch (transition) {
    case GST_STATE_CHANGE_READY_TO_NULL: {
        gst_gva_identify_reset(gvaidentify);
        break;
    }
    default:
        break;
    }

    return ret;
}

static void gst_gva_identify_init(GstGvaIdentify *gvaidentify) {

    GST_DEBUG_OBJECT(gvaidentify, "gst_gva_identify_init");
    GST_DEBUG_OBJECT(gvaidentify, "%s", GST_ELEMENT_NAME(GST_ELEMENT(gvaidentify)));

    gst_gva_identify_reset(gvaidentify);
}

void gst_gva_identify_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(object);

    GST_DEBUG_OBJECT(gvaidentify, "set_property");

    switch (property_id) {
    case PROP_MODEL:
        gvaidentify->model = g_value_dup_string(value);
        break;
    case PROP_GALLERY:
        gvaidentify->gallery = g_value_dup_string(value);
        break;
    case PROP_TRACKER:
        gvaidentify->tracker = g_value_get_boolean(value);
        break;
    case PROP_THRESHOLD:
        gvaidentify->threshold = g_value_get_float(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_identify_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(object);

    GST_DEBUG_OBJECT(gvaidentify, "get_property");

    switch (property_id) {
    case PROP_MODEL:
        g_value_set_string(value, gvaidentify->model);
        break;
    case PROP_THRESHOLD:
        g_value_set_float(value, gvaidentify->threshold);
        break;
    case PROP_GALLERY:
        g_value_set_string(value, gvaidentify->gallery);
        break;
    case PROP_TRACKER:
        g_value_set_boolean(value, gvaidentify->tracker);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_gva_identify_dispose(GObject *object) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(object);

    GST_DEBUG_OBJECT(gvaidentify, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_identify_parent_class)->dispose(object);
}

void gst_gva_identify_finalize(GObject *object) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(object);

    GST_DEBUG_OBJECT(gvaidentify, "finalize");

    /* clean up object here */
    gst_gva_identify_cleanup(gvaidentify);

    G_OBJECT_CLASS(gst_gva_identify_parent_class)->finalize(object);
}

static gboolean gst_gva_identify_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(trans);

    GST_DEBUG_OBJECT(gvaidentify, "set_caps");

    if (!gvaidentify->info) {
        gvaidentify->info = gst_video_info_new();
    }
    gst_video_info_from_caps(gvaidentify->info, incaps);

    return TRUE;
}

/* states */
static gboolean gst_gva_identify_start(GstBaseTransform *trans) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(trans);

    GST_DEBUG_OBJECT(gvaidentify, "start");

    if (gvaidentify->initialized)
        goto exit;

    if (!gvaidentify->identifier) {
        gvaidentify->identifier = identifier_new(gvaidentify);
    }
    gvaidentify->initialized = TRUE;
exit:
    return gvaidentify->initialized;
}

static gboolean gst_gva_identify_stop(GstBaseTransform *trans) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(trans);

    GST_DEBUG_OBJECT(gvaidentify, "stop");
    return TRUE;
}

static gboolean gst_gva_identify_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(trans);

    GST_DEBUG_OBJECT(gvaidentify, "sink_event");
    return GST_BASE_TRANSFORM_CLASS(gst_gva_identify_parent_class)->sink_event(trans, event);
}

static GstFlowReturn gst_gva_identify_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GstGvaIdentify *gvaidentify = GST_GVA_IDENTIFY(trans);

    GST_DEBUG_OBJECT(gvaidentify, "transform_ip");
    return frame_to_identify(gvaidentify, buf, gvaidentify->info);
}
