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

void create_iterative_speedometer(const char *intervals);
// void create_average_speedometer(unsigned int skip_frames);
void speedometer_new_frame(GstBuffer *buf, const char *element_name);
void speedometer_eos();

#ifdef __cplusplus
}
#endif

#endif /* __WATERMARK_H__ */
