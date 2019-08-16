/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"
#include <gst/video/video-info.h>

#define SYSTEM_MEM_CAPS GST_VIDEO_CAPS_MAKE("{ BGRx, BGRA }") "; "

#ifdef SUPPORT_DMA_BUFFER
#define DMA_BUFFER_CAPS GST_VIDEO_CAPS_MAKE_WITH_FEATURES("memory:DMABuf", "{ NV12, RGBA }") "; "
#else
#define DMA_BUFFER_CAPS
#endif

#define GVA_CAPS SYSTEM_MEM_CAPS DMA_BUFFER_CAPS
