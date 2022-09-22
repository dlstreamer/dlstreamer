/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "video_inference.h"

#include <string>

G_BEGIN_DECLS

#define VIDEO_CLASSIFY_NAME "Object classification (requires GstVideoRegionOfInterestMeta on input)"
#define VIDEO_CLASSIFY_DESCRIPTION                                                                                     \
    "Performs object classification. Accepts the ROI or full frame as an input and "                                   \
    "outputs classification results with metadata."

GST_DEBUG_CATEGORY_EXTERN(video_classify_debug_category);

GType video_classify_get_type(void);

G_END_DECLS
