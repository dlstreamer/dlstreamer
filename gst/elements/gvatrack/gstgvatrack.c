/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvatrack.h"
#include "tracker_c.h"

#define ELEMENT_LONG_NAME "Object tracker (generates GstGvaObjectTrackerMeta, GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION "Object tracker (generates GstGvaObjectTrackerMeta, GstVideoRegionOfInterestMeta)"

#define UNUSED(x) (void)(x)

GST_DEBUG_CATEGORY_STATIC(gst_gva_track_debug_category);
#define GST_CAT_DEFAULT gst_gva_track_debug_category

#define DEFAULT_TRACKING_TYPE IOU

enum {
    PROP_0,
    PROP_TRACKING_TYPE,
};

/// the capabilities of the inputs and outputs.
#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR }") "; "
#define VIDEO_SINK_CAPS SYSTEM_MEM_CAPS
#define VIDEO_SRC_CAPS SYSTEM_MEM_CAPS

G_DEFINE_TYPE_WITH_CODE(GstGvaTrack, gst_gva_track, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_track_debug_category, "gvatrack", 0,
                                                "debug category for gvatrack element"));

static void gst_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_dispose(GObject *object);
static void gst_finalize(GObject *object);

static gboolean gst_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_sink_event(GstBaseTransform *trans, GstEvent *event);
static gboolean gst_start(GstBaseTransform *trans);
static gboolean gst_stop(GstBaseTransform *trans);
static GstFlowReturn gst_transform_ip(GstBaseTransform *trans, GstBuffer *buf);

static GstStateChangeReturn gst_change_state(GstElement *element, GstStateChange transition);

static void gst_gva_track_class_init(GstGvaTrackClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_set_property);
    gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_get_property);
    gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_dispose);
    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_finalize);

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_set_caps);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_sink_event);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_stop);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_transform_ip);

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_change_state);

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(VIDEO_SRC_CAPS)));
    gst_element_class_add_pad_template(element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                                           gst_caps_from_string(VIDEO_SINK_CAPS)));

    const GParamFlags kDefaultGParamFlags = (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_object_class_install_property(gobject_class, PROP_TRACKING_TYPE,
                                    g_param_spec_enum("tracking-type", "TrackingType", "Tracking type",
                                                      GST_GVA_TRACKING_TYPE, DEFAULT_TRACKING_TYPE,
                                                      kDefaultGParamFlags));
}

static void gst_gva_track_init(GstGvaTrack *gva_track) {
    gva_track->tracking_type = DEFAULT_TRACKING_TYPE;
    gva_track->tracker = NULL;
}

static void gst_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);
    GST_DEBUG_OBJECT(gva_track, "gst_set_property %d", prop_id);

    switch (prop_id) {
    case PROP_TRACKING_TYPE:
        gva_track->tracking_type = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);

    switch (prop_id) {
    case PROP_TRACKING_TYPE:
        g_value_set_enum(value, gva_track->tracking_type);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_dispose(GObject *object) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);

    GST_DEBUG_OBJECT(gva_track, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_track_parent_class)->dispose(object);
}

void gst_finalize(GObject *object) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);
    release_tracker_instance(gva_track->tracker);
    gva_track->tracker = NULL;

    G_OBJECT_CLASS(gst_gva_track_parent_class)->finalize(object);
}

static GstStateChangeReturn gst_change_state(GstElement *element, GstStateChange transition) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(element);

    GstStateChangeReturn ret = GST_ELEMENT_CLASS(gst_gva_track_parent_class)->change_state(element, transition);
    GST_DEBUG_OBJECT(gva_track, "GstStateChangeReturn: %s", gst_element_state_change_return_get_name(ret));

    return ret;
}

static gboolean gst_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaTrack *gva_track = GST_GVA_TRACK(trans);
    GST_DEBUG_OBJECT(gva_track, "gst_set_caps");

    if (!gva_track->info) {
        gva_track->info = gst_video_info_new();
    }
    gst_video_info_from_caps(gva_track->info, incaps);
    if (gva_track->tracker != NULL) {
        release_tracker_instance(gva_track->tracker);
    }
    if (gva_track->tracker == NULL) {
        GError *error = NULL;
        gva_track->tracker = acquire_tracker_instance(gva_track->info, gva_track->tracking_type, &error);
        if (error) {
            GST_ELEMENT_ERROR(gva_track, RESOURCE, TOO_LAZY, ("tracker intitialization failed"),
                              ("%s", error->message));
            g_error_free(error);
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean gst_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(trans);

    GST_DEBUG_OBJECT(gva_track, "sink_event %s", GST_EVENT_TYPE_NAME(event));

    return GST_BASE_TRANSFORM_CLASS(gst_gva_track_parent_class)->sink_event(trans, event);
}

static gboolean gst_start(GstBaseTransform *trans) {
    UNUSED(trans);
    return TRUE;
}

static gboolean gst_stop(GstBaseTransform *trans) {
    UNUSED(trans);
    return TRUE;
}

/* GstElement vmethod implementations */

static GstFlowReturn gst_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GError *error = NULL;
    GstGvaTrack *gva_track = GST_GVA_TRACK(trans);
    GstFlowReturn status = GST_FLOW_ERROR;
    if (gva_track->tracker) {
        transform_tracked_objects(gva_track->tracker, buf, &error);
        if (error) {
            GST_ELEMENT_ERROR(gva_track, RESOURCE, TOO_LAZY, ("transform_ip failed"), ("%s", error->message));
            g_error_free(error);
            status = GST_FLOW_ERROR;
        } else {
            status = GST_FLOW_OK;
        }
    }
    return status;
}
