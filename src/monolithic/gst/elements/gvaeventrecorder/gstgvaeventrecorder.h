/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_GVAEVENTRECORDER (gst_gvaeventrecorder_get_type())
#define GST_GVAEVENTRECORDER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVAEVENTRECORDER, GstGvaEventRecorder))
#define GST_GVAEVENTRECORDER_CLASS(klass)                                                                             \
    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVAEVENTRECORDER, GstGvaEventRecorderClass))
#define GST_IS_GVAEVENTRECORDER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVAEVENTRECORDER))
#define GST_IS_GVAEVENTRECORDER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVAEVENTRECORDER))

typedef struct _GstGvaEventRecorder GstGvaEventRecorder;
typedef struct _GstGvaEventRecorderClass GstGvaEventRecorderClass;

// Cooldown unit, mirrors gvaeventrecorder::CooldownUnit (property enum needs its own GType).
typedef enum { GST_GVAEVENTRECORDER_COOLDOWN_FRAMES = 0, GST_GVAEVENTRECORDER_COOLDOWN_TIME = 1 } GstGvaEventRecorderCooldownUnit;

// Encoder backend for the video dump, mirrors gvaeventrecorder::EncoderMode.
typedef enum {
    GST_GVAEVENTRECORDER_ENCODER_SOFTWARE = 0,
    GST_GVAEVENTRECORDER_ENCODER_VAAPI = 1,
    GST_GVAEVENTRECORDER_ENCODER_AUTO = 2
} GstGvaEventRecorderEncoderMode;

// Output container, mirrors gvaeventrecorder::ContainerFormat.
typedef enum { GST_GVAEVENTRECORDER_CONTAINER_MP4 = 0, GST_GVAEVENTRECORDER_CONTAINER_MKV = 1 } GstGvaEventRecorderContainer;

struct _GstGvaEventRecorder {
    GstBaseTransform element;

    // Properties (CLI- and runtime-configurable).
    gchar *target_classes; // comma-separated class labels
    gdouble confidence_threshold;
    guint pre_event_frames;
    guint post_event_frames;
    guint event_gap_frames;
    gint cooldown_unit; // GstGvaEventRecorderCooldownUnit
    guint64 cooldown_value;
    guint max_concurrent_events;
    gchar *full_clip_output_dir;
    gchar *downstream_output_dir;
    gchar *location_template;
    gint encoder_mode; // GstGvaEventRecorderEncoderMode
    gint container;    // GstGvaEventRecorderContainer
    guint drop_log_interval_ms;
    guint64 frame_counter; // tick source when cooldown_unit == FRAMES

    void *priv; // GvaEventRecorderRuntime* (opaque; allocated/freed in gstgvaeventrecorder.cpp)
};

struct _GstGvaEventRecorderClass {
    GstBaseTransformClass parent_class;
};

GType gst_gvaeventrecorder_get_type(void);

G_END_DECLS
