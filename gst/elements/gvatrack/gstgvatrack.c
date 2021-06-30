/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gstgvatrack.h"
#include "tracker_c.h"
#include "utils.h"

#define ELEMENT_LONG_NAME "Object tracker (generates GstGvaObjectTrackerMeta, GstVideoRegionOfInterestMeta)"
#define ELEMENT_DESCRIPTION                                                                                            \
    "Performs object tracking using zero-term, zero-term-imageless, short-term, or short-term-imageless tracking "     \
    "algorithms. Zero-term tracking assigns unique object IDs and requires object detection to run on every frame. "   \
    "Short-term tracking allows to track objects between frames, thereby reducing the need to run object detection "   \
    "on each frame. Imageless tracking (zero-term-imageless and short-term-imageless) forms object associations "      \
    "based on the movement and shape of objects, and it does not use image data."

GST_DEBUG_CATEGORY(gst_gva_track_debug_category);

#define DEFAULT_DEVICE ""
#define DEFAULT_TRACKING_TYPE SHORT_TERM
#define DEFAULT_TRACKING_CONFIG NULL

#define DEVICE_CPU "CPU"
#define DEVICE_GPU "GPU"

enum {
    PROP_0,
    PROP_DEVICE,
    PROP_TRACKING_TYPE,
    PROP_TRACKING_CONFIG,
};

G_DEFINE_TYPE_WITH_CODE(GstGvaTrack, gst_gva_track, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_gva_track_debug_category, "gvatrack", 0,
                                                "debug category for gvatrack element"));

static void gst_gva_track_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gva_track_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void gst_gva_track_dispose(GObject *object);
static void gst_gva_track_finalize(GObject *object);

static void gst_gva_track_cleanup(GstGvaTrack *gva_track);
static gboolean gst_gva_track_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps);
static gboolean gst_gva_track_sink_event(GstBaseTransform *trans, GstEvent *event);
static gboolean gst_gva_track_start(GstBaseTransform *trans);
static gboolean gst_gva_track_stop(GstBaseTransform *trans);
static GstFlowReturn gst_gva_track_transform_ip(GstBaseTransform *trans, GstBuffer *buf);

static GstStateChangeReturn gst_gva_track_change_state(GstElement *element, GstStateChange transition);

void gst_gva_track_cleanup(GstGvaTrack *gva_track) {
    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_cleanup");

    if (gva_track == NULL)
        return;

    release_tracker_instance(gva_track->tracker);
    gva_track->tracker = NULL;

    g_free(gva_track->device);
    gva_track->device = NULL;

    if (gva_track->info) {
        gst_video_info_free(gva_track->info);
        gva_track->info = NULL;
    }
}

static void gst_gva_track_class_init(GstGvaTrackClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = GST_DEBUG_FUNCPTR(gst_gva_track_set_property);
    gobject_class->get_property = GST_DEBUG_FUNCPTR(gst_gva_track_get_property);
    gobject_class->dispose = GST_DEBUG_FUNCPTR(gst_gva_track_dispose);
    gobject_class->finalize = GST_DEBUG_FUNCPTR(gst_gva_track_finalize);

    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    base_transform_class->set_caps = GST_DEBUG_FUNCPTR(gst_gva_track_set_caps);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(gst_gva_track_sink_event);
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_gva_track_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_gva_track_stop);
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_gva_track_transform_ip);

    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    element_class->change_state = GST_DEBUG_FUNCPTR(gst_gva_track_change_state);

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string(GVA_CAPS)));

    const GParamFlags kDefaultGParamFlags = (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property(
        gobject_class, PROP_TRACKING_TYPE,
        g_param_spec_enum("tracking-type", "TrackingType",
                          "Tracking algorithm used to identify the same object in multiple frames. "
                          "Please see user guide for more details",
                          GST_GVA_TRACKING_TYPE, DEFAULT_TRACKING_TYPE, kDefaultGParamFlags));
    g_object_class_install_property(gobject_class, PROP_DEVICE,
                                    g_param_spec_string("device", "Device",
                                                        "Target device for tracking. Supported devices are CPU, "
                                                        "GPU.<id>, where id is GPU device id "
                                                        "and VPU.<id>, where id is VPU slice id.",
                                                        DEFAULT_DEVICE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_TRACKING_CONFIG,
                                    g_param_spec_string("config", "Tracker specific configuration",
                                                        "Comma separated list of KEY=VALUE parameters specific to "
                                                        "platform/tracker. Please see user guide for more details",
                                                        DEFAULT_TRACKING_CONFIG,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_gva_track_init(GstGvaTrack *gva_track) {
    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_init");

    if (gva_track == NULL)
        return;

    gst_gva_track_cleanup(gva_track);

    gva_track->tracking_type = DEFAULT_TRACKING_TYPE;
    gva_track->device = g_strdup(DEFAULT_DEVICE);
    gva_track->tracking_config = g_strdup(DEFAULT_TRACKING_CONFIG);
}

static void gst_gva_track_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);
    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_set_property %d", prop_id);

    switch (prop_id) {
    case PROP_DEVICE:
        g_free(gva_track->device);
        gva_track->device = g_ascii_strup(g_value_dup_string(value), -1);
        break;
    case PROP_TRACKING_TYPE:
        gva_track->tracking_type = g_value_get_enum(value);
        break;
    case PROP_TRACKING_CONFIG:
        g_free(gva_track->tracking_config);
        gva_track->tracking_config = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void gst_gva_track_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);
    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_get_property %d", prop_id);

    switch (prop_id) {
    case PROP_DEVICE:
        g_value_set_string(value, gva_track->device);
        break;
    case PROP_TRACKING_TYPE:
        g_value_set_enum(value, gva_track->tracking_type);
        break;
    case PROP_TRACKING_CONFIG:
        g_value_set_string(value, gva_track->tracking_config);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

void gst_gva_track_dispose(GObject *object) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);

    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_gva_track_parent_class)->dispose(object);
}

void gst_gva_track_finalize(GObject *object) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(object);
    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_finalize");

    gst_gva_track_cleanup(gva_track);

    G_OBJECT_CLASS(gst_gva_track_parent_class)->finalize(object);
}

static GstStateChangeReturn gst_gva_track_change_state(GstElement *element, GstStateChange transition) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(element);

    GstStateChangeReturn ret = GST_ELEMENT_CLASS(gst_gva_track_parent_class)->change_state(element, transition);
    GST_DEBUG_OBJECT(gva_track, "GstStateChangeReturn: %s", gst_element_state_change_return_get_name(ret));

    return ret;
}

static gboolean _check_device_correctness(GstGvaTrack *gva_track) {
    return strcmp(gva_track->device, DEVICE_GPU) == 0 && gva_track->caps_feature != VA_SURFACE_CAPS_FEATURE &&
           gva_track->caps_feature != DMA_BUF_CAPS_FEATURE;
}

static void _try_to_create_default_gpu_tracker(GstGvaTrack *gva_track) {
    // Set default device if it wasn't specified by user
    if (gva_track->device != NULL && gva_track->device[0] != '\0')
        return;
    gboolean tryGPU = gva_track->tracking_type == ZERO_TERM && (gva_track->caps_feature == VA_SURFACE_CAPS_FEATURE ||
                                                                gva_track->caps_feature == DMA_BUF_CAPS_FEATURE);
    tryGPU = FALSE; // disable default loading of libvasot_gpu, will be removed
    if (tryGPU) {
        gva_track->device = g_strdup(DEVICE_GPU);

        GError *error = NULL;
        gva_track->tracker = acquire_tracker_instance(gva_track, &error);
        if (error) {
            GST_ELEMENT_INFO(gva_track, LIBRARY, INIT, ("can't init tracker to run on GPU"), ("%s", error->message));
            release_tracker_instance(gva_track->tracker);
            gva_track->tracker = NULL;
            g_error_free(error);
        } else {
            GST_ELEMENT_INFO(gva_track, LIBRARY, INIT, ("initialized GPU tracker instance"), NULL);
        }
    }

    if (gva_track->tracker == NULL)
        gva_track->device = g_strdup(DEVICE_CPU);
}

static gboolean gst_gva_track_set_caps(GstBaseTransform *trans, GstCaps *incaps, GstCaps *outcaps) {
    UNUSED(outcaps);

    GstGvaTrack *gva_track = GST_GVA_TRACK(trans);
    GST_DEBUG_OBJECT(gva_track, "gst_gva_track_set_caps");

    if (!gva_track->info) {
        gva_track->info = gst_video_info_new();
    }
    gst_video_info_from_caps(gva_track->info, incaps);
    if (gva_track->tracker != NULL) {
        release_tracker_instance(gva_track->tracker);
        gva_track->tracker = NULL;
    }
    gva_track->caps_feature = get_caps_feature(incaps);

    _try_to_create_default_gpu_tracker(gva_track);

    if (_check_device_correctness(gva_track)) {
        GST_ELEMENT_ERROR(gva_track, LIBRARY, INIT, ("tracker intitialization failed"),
                          ("memory type should be VASurface or DMABuf for running on GPU"));
        return FALSE;
    }

    if (gva_track->tracker == NULL) {
        GError *error = NULL;
        gva_track->tracker = acquire_tracker_instance(gva_track, &error);
        if (error) {
            GST_ELEMENT_ERROR(gva_track, LIBRARY, INIT, ("tracker intitialization failed"), ("%s", error->message));
            g_error_free(error);
            return FALSE;
        } else {
            GST_ELEMENT_INFO(gva_track, LIBRARY, INIT, ("initialized %s tracker instance", gva_track->device), NULL);
        }
    }

    return TRUE;
}

static gboolean gst_gva_track_sink_event(GstBaseTransform *trans, GstEvent *event) {
    GstGvaTrack *gva_track = GST_GVA_TRACK(trans);

    GST_DEBUG_OBJECT(gva_track, "sink_event %s", GST_EVENT_TYPE_NAME(event));

    return GST_BASE_TRANSFORM_CLASS(gst_gva_track_parent_class)->sink_event(trans, event);
}

static gboolean gst_gva_track_start(GstBaseTransform *trans) {
    UNUSED(trans);
    return TRUE;
}

static gboolean gst_gva_track_stop(GstBaseTransform *trans) {
    UNUSED(trans);
    return TRUE;
}

static GstFlowReturn gst_gva_track_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    GError *error = NULL;
    GstGvaTrack *gva_track = GST_GVA_TRACK(trans);
    GstFlowReturn status = GST_FLOW_OK;
    if (gva_track && gva_track->tracker) {
        transform_tracked_objects(gva_track->tracker, buf, &error);
        if (error) {
            GST_ELEMENT_ERROR(gva_track, STREAM, FAILED, ("transform_ip failed"), ("%s", error->message));
            g_error_free(error);
            status = GST_FLOW_ERROR;
        }
    } else {
        GST_ELEMENT_ERROR(gva_track, STREAM, FAILED, ("transform_ip failed"), ("%s", "bad argument gva_track"));
        status = GST_FLOW_ERROR;
    }
    return status;
}
