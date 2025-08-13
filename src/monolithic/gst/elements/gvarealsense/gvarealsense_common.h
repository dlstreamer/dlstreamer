/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GVAREALSENSE_COMMON_H__
#define __GVAREALSENSE_COMMON_H__

#include <glib.h>
#include <gst/gst.h>

struct PointXYZRGB {
    float x, y, z;
    guint8 r, g, b;
};

#endif // __GVAREALSENSE_COMMON_H__