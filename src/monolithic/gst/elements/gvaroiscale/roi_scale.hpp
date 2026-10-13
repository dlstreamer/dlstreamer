/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/analytics/analytics.h>
#include <gst/video/video.h>

namespace gvaroiscale {

enum class RoiScaleResult { Ok, NoRoi, Error };

RoiScaleResult roi_scale_buffer(GstBuffer *input_buffer, const GstVideoInfo &input_info, guint output_width,
                                guint output_height, GstBuffer **output_buffer, GstVideoInfo *output_info);

} // namespace gvaroiscale