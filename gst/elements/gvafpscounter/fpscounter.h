/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __FPSCOUNTER_H__
#define __FPSCOUNTER_H__

#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif

void create_iterative_fps_counter(const char *intervals);
void create_average_fps_counter(unsigned int skip_frames);
void fps_counter_new_frame(GstBuffer *, const char *element_name);
void fps_counter_eos();

#ifdef __cplusplus
}
#endif

#endif /* __FPSCOUNTER_H__ */
