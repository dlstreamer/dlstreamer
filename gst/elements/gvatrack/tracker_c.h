/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
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

ITracker *acquire_tracker_instance(const GstVideoInfo *info, GstGvaTrackingType tracking_type, GError **error);
void transform_tracked_objects(ITracker *tracker, GstBuffer *buffer, GError **error);
void release_tracker_instance(ITracker *tracker);

G_END_DECLS
