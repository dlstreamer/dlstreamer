/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "tracker_c.h"
#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_TRACK (gst_gva_track_get_type())
#define GST_GVA_TRACK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_TRACK, GstGvaTrack))
#define GST_GVA_TRACK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_TRACK, GstGvaTrackClass))
#define GST_IS_GVA_TRACK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_TRACK))
#define GST_IS_GVA_TRACK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_TRACK))

typedef struct _GstGvaTrack {
    GstBaseTransform base_transform;
    GstVideoInfo *info;

    GstGvaTrackingType tracking_type;

    ITracker *tracker;
} GstGvaTrack;

typedef struct _GstGvaTrackClass {
    GstBaseTransformClass base_transform_class;
} GstGvaTrackClass;

GType gst_gva_track_get_type(void);

G_END_DECLS
