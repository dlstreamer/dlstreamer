/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __FPS_METER_H__
#define __FPS_METER_H__

#include <time.h>

#define FPS_METER_DEFAULT_INTERVAL 1000 /* ms */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fps_meter_s {
    struct timespec last_clock;
    int frames;
    float fps;
} fps_meter_s;

void fps_meter_init();
int fps_meter_new_frame(fps_meter_s *fps_meter, int interval_ms);
#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* __FPS_METER_H__ */
