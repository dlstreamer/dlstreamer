/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __WATERMARK_H__
#define __WATERMARK_H__

#include "gva_utils.h"
#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif

void draw_label(GstBuffer *buffer, GstVideoInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* __WATERMARK_H__ */
