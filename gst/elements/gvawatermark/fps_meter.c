/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <math.h>
#include <time.h>

#include "fps_meter.h"

static inline struct timespec timespec_subtract(struct timespec lhs, struct timespec rhs);
static inline long timespec_to_ms(struct timespec time);

void fps_meter_init(fps_meter_s *fps_meter) {
    struct timespec current_time = {};
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    fps_meter->last_clock = current_time;
    fps_meter->frames = 0;
    fps_meter->fps = 0;
}

int fps_meter_new_frame(fps_meter_s *fps_meter, int interval_ms) {
    struct timespec current_time;

    clock_gettime(CLOCK_MONOTONIC, &current_time);
    fps_meter->frames++;

    long ms = timespec_to_ms(timespec_subtract(current_time, fps_meter->last_clock));

    if (ms >= interval_ms) {
        fps_meter->fps = fps_meter->frames * 1000 / ms;
        fps_meter->frames = 0;
        fps_meter->last_clock = current_time;
        return 1;
    }
    return 0;
};

static inline struct timespec timespec_subtract(struct timespec time1, struct timespec time2) {
    struct timespec retval = {};

    if ((time1.tv_sec < time2.tv_sec) || ((time1.tv_sec == time2.tv_sec) && (time1.tv_nsec <= time2.tv_nsec))) {
        return retval;
    }

    retval.tv_sec = time1.tv_sec - time2.tv_sec;
    if (time1.tv_nsec < time2.tv_nsec) {
        retval.tv_nsec = time1.tv_nsec + 1000000000L - time2.tv_nsec;
        retval.tv_sec--;
    } else {
        retval.tv_nsec = time1.tv_nsec - time2.tv_nsec;
    }
    return retval;
}

static inline long timespec_to_ms(struct timespec time) {
    return time.tv_sec * 1000 + lround(time.tv_nsec / 1.E6);
}
