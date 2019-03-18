/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"
#include "inference_backend/image_inference.h"
#include <gst/gstbuffer.h>
#include <gst/video/video-info.h>

#ifndef DISABLE_VAAPI
#include <va/va.h>
#endif

struct BufferMapContext {
    GstMapInfo gstMapInfo;
#ifndef DISABLE_VAAPI
    VADisplay va_display;
    VAImage va_image;
#endif
};

bool gva_buffer_map(GstBuffer *buffer, InferenceBackend::Image &image, BufferMapContext &mapContext, GstVideoInfo *info,
                    InferenceBackend::MemoryType memoryType);

void gva_buffer_unmap(GstBuffer *buffer, InferenceBackend::Image &image, BufferMapContext &mapContext);
