/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "tracker_types.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#ifdef __cplusplus
class ITracker;
#else  /* __cplusplus */
typedef struct ITracker ITracker;
#endif /* __cplusplus */

G_BEGIN_DECLS

struct _GstGvaTrack;
typedef struct _GstGvaTrack GstGvaTrack;

ITracker *acquire_tracker_instance(const GstGvaTrack *gva_track, GError **error);
void transform_tracked_objects(ITracker *tracker, GstBuffer *buffer, GError **error);
void release_tracker_instance(ITracker *tracker);

G_END_DECLS
