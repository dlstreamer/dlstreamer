/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __WATERMARK_H__
#define __WATERMARK_H__

#include "gstgvawatermark.h"
#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif

gboolean draw_label(GstGvaWatermark *gvawatermark, GstBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __WATERMARK_H__ */
