/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include <gst/gstcaps.h>

#define DMABUF_FEATURE_STR "memory:DMABuf"
#define VASURFACE_FEATURE_STR "memory:VASurface"

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA, BGR, NV12, I420 }") "; "

#ifdef ENABLE_VAAPI
#define VASURFACE_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES(VASURFACE_FEATURE_STR, "{ NV12 }") "; "
#else
#define VASURFACE_CAPS
#endif

#if (defined ENABLE_VPUX || defined ENABLE_VAAPI)
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES(DMABUF_FEATURE_STR, "{ NV12, RGBA, I420 }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#define GVA_CAPS SYSTEM_MEM_CAPS DMA_BUFFER_CAPS VASURFACE_CAPS
typedef enum {
    SYSTEM_MEMORY_CAPS_FEATURE,
    VA_SURFACE_CAPS_FEATURE,
    DMA_BUF_CAPS_FEATURE,
} CapsFeature;

#ifdef __cplusplus
extern "C" {
#endif
CapsFeature get_caps_feature(GstCaps *caps);
#ifdef __cplusplus
}
#endif
