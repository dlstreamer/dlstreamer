/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "video_inference.h"

#include <string>

G_BEGIN_DECLS

#define VIDEO_DETECT_NAME "Object detection (generates GstVideoRegionOfInterestMeta)"
#define VIDEO_DETECT_DESCRIPTION "Performs inference-based object detection"

GST_DEBUG_CATEGORY_EXTERN(video_detect_debug_category);

GType video_detect_get_type(void);

G_END_DECLS
