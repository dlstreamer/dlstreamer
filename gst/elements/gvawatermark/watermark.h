/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __WATERMARK_H__
#define __WATERMARK_H__

#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif

void draw_label(GstBuffer *buffer, GstVideoInfo *info);
void print_points_with_id(GstBuffer *buffer, GstVideoInfo *info);
#ifdef __cplusplus
}
#endif

#endif /* __WATERMARK_H__ */
