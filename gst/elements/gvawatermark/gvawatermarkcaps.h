/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gva_caps.h"

// Preferred format to use with VASurface and DMABuf
#define WATERMARK_PREFERRED_REMOTE_FORMAT "RGBA"

// Capabilities
#ifdef ENABLE_VAAPI
#define WATERMARK_VASURFACE_CAPS                                                                                       \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES(VASURFACE_FEATURE_STR, WATERMARK_PREFERRED_REMOTE_FORMAT) "; "
#define WATERMARK_DMA_BUFFER_CAPS                                                                                      \
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES(DMABUF_FEATURE_STR, WATERMARK_PREFERRED_REMOTE_FORMAT) "; "
#else
#define WATERMARK_VASURFACE_CAPS
#define WATERMARK_DMA_BUFFER_CAPS
#endif

#define WATERMARK_ALL_CAPS SYSTEM_MEM_CAPS WATERMARK_VASURFACE_CAPS WATERMARK_DMA_BUFFER_CAPS
