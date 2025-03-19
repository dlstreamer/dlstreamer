/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_frames_buffer.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>

#define ELEMENT_LONG_NAME "Buffer and optionally repeat compressed video frames"
#define ELEMENT_DESCRIPTION ELEMENT_LONG_NAME

GST_DEBUG_CATEGORY_STATIC(video_frames_buffer_debug_category);
#define GST_CAT_DEFAULT video_frames_buffer_debug_category

enum { PROP_0, PROP_NUMBER_INPUT_FRAMES, PROP_NUMBER_OUTPUT_FRAMES };

/* prototypes */
static void video_frames_buffer_set_property(GObject *object, guint property_id, const GValue *value,
                                             GParamSpec *pspec);
static gboolean video_frames_buffer_start(GstBaseTransform *trans);
static void video_frames_buffer_dispose(GObject *object);
static void video_frames_buffer_finalize(GObject *object);

static GstFlowReturn video_frames_buffer_transform_ip(GstBaseTransform *trans, GstBuffer *buf);
static gboolean video_frames_buffer_sink_event(GstBaseTransform *trans, GstEvent *event);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(VideoFramesBuffer, video_frames_buffer, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(video_frames_buffer_debug_category, "VIDEO_FRAMES_BUFFER", 0,
                                                "debug category for self element"));
static void video_frames_buffer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static void video_frames_buffer_class_init(VideoFramesBufferClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class = GST_BASE_TRANSFORM_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_caps_from_string("ANY")));

    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string("ANY")));
    gst_element_class_add_pad_template(
        element_class, gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS, gst_caps_from_string("ANY")));

    gst_element_class_set_static_metadata(element_class, ELEMENT_LONG_NAME, "Video", ELEMENT_DESCRIPTION,
                                          "Intel Corporation");
    gobject_class->set_property = video_frames_buffer_set_property;
    gobject_class->get_property = video_frames_buffer_get_property;
    gobject_class->dispose = video_frames_buffer_dispose;
    gobject_class->finalize = video_frames_buffer_finalize;
    base_transform_class->start = GST_DEBUG_FUNCPTR(video_frames_buffer_start);
    base_transform_class->transform = NULL;
    base_transform_class->transform_ip = GST_DEBUG_FUNCPTR(video_frames_buffer_transform_ip);
    base_transform_class->sink_event = GST_DEBUG_FUNCPTR(video_frames_buffer_sink_event);
    g_object_class_install_property(gobject_class, PROP_NUMBER_INPUT_FRAMES,
                                    g_param_spec_int("num-input-frames", "Number input frames to buffer",
                                                     "Number input frames to buffer", 0, INT_MAX, 0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(gobject_class, PROP_NUMBER_OUTPUT_FRAMES,
                                    g_param_spec_int("num-output-frames", "Max number output frames in 'loop' mode",
                                                     "Max number output frames in 'loop' mode", 0, INT_MAX, 0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void video_frames_buffer_init(VideoFramesBuffer *self) {
    self->number_input_frames = 0;
    self->number_output_frames = 0;
    self->buffers = NULL;
    self->curr_input_frames = 0;
    self->curr_output_frames = 0;
    self->last_pts = 0;
    self->pts_delta = 0;
}

void video_frames_buffer_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec) {
    VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(object);

    GST_DEBUG_OBJECT(self, "set_property");

    switch (property_id) {
    case PROP_NUMBER_INPUT_FRAMES:
        self->number_input_frames = g_value_get_int(value);
        break;
    case PROP_NUMBER_OUTPUT_FRAMES:
        self->number_output_frames = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void video_frames_buffer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
    VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(object);

    GST_DEBUG_OBJECT(self, "get_property");

    switch (prop_id) {
    case PROP_NUMBER_INPUT_FRAMES: {
        g_value_set_int(value, self->number_input_frames);
        break;
    }
    case PROP_NUMBER_OUTPUT_FRAMES: {
        g_value_set_int(value, self->number_output_frames);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean video_frames_buffer_start(GstBaseTransform *trans) {
    VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(trans);
    GST_DEBUG_OBJECT(self, "start");

    if (!self->buffers)
        self->buffers = calloc(self->number_input_frames, sizeof(GstBuffer *));

    return TRUE;
}

void video_frames_buffer_dispose(GObject *object) {
    VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(object);
    GST_DEBUG_OBJECT(self, "dispose");

    G_OBJECT_CLASS(video_frames_buffer_parent_class)->dispose(object);
}

void video_frames_buffer_finalize(GObject *object) {
    VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(object);
    GST_DEBUG_OBJECT(self, "finalize");

    for (int i = 0; i < self->number_input_frames; i++) {
        if (self->buffers[i]) {
            gst_buffer_unref(self->buffers[i]);
            self->buffers[i] = NULL;
        }
    }

    G_OBJECT_CLASS(video_frames_buffer_parent_class)->finalize(object);
}

static GstFlowReturn video_frames_buffer_loop_frames(VideoFramesBuffer *self) {
    GstClockTime pts = self->last_pts;
    int i = 0;
    for (;;) {
        GstBuffer *out = gst_buffer_ref(self->buffers[i]);
        pts += self->pts_delta;
        GST_BUFFER_PTS(out) = pts;
        gst_pad_push(GST_BASE_TRANSFORM_SRC_PAD(self), out);
        i++;
        if (i >= self->number_input_frames || !self->buffers[i])
            i = 0;
        // EOS
        if (++self->curr_output_frames >= self->number_output_frames) {
            gst_pad_send_event(self->base_transform.sinkpad, gst_event_new_eos());
            return GST_BASE_TRANSFORM_FLOW_DROPPED;
        }
    }
}

static GstFlowReturn video_frames_buffer_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
    VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(trans);
    GST_DEBUG_OBJECT(self, "transform_ip");

    if (!self->number_input_frames)
        return GST_FLOW_OK;
    // if (GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_HEADER))
    //    return GST_FLOW_OK;
    // if (!GST_BUFFER_FLAG_IS_SET(buf, GST_BUFFER_FLAG_DELTA_UNIT))
    //    printf("$$$$$$$$$$$$$$$$$ key frame\n");

    if (self->curr_input_frames < self->number_input_frames) {
        self->buffers[self->curr_input_frames++] = gst_buffer_ref(buf);
        GstClockTime pts = GST_BUFFER_PTS(buf);
        if (self->last_pts)
            self->pts_delta = pts - self->last_pts;
        self->last_pts = pts;
        self->curr_output_frames++;
    }

    if (self->curr_input_frames >= self->number_input_frames) {
        return video_frames_buffer_loop_frames(self);
    } else {
        return GST_FLOW_OK;
    }
}

gboolean video_frames_buffer_sink_event(GstBaseTransform *trans, GstEvent *event) {
    // VideoFramesBuffer *self = VIDEO_FRAMES_BUFFER(trans);
    // int etype = GST_EVENT_TYPE(event);
    // if (etype == GST_EVENT_EOS) {
    //    video_frames_buffer_loop_frames(self);
    //    return TRUE;
    //}
    return GST_BASE_TRANSFORM_CLASS(video_frames_buffer_parent_class)->sink_event(trans, event);
}
