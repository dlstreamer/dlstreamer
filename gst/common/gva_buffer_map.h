/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"
#include "inference_backend/image.h"
#include <gst/gstbuffer.h>
#include <gst/video/video-info.h>

struct BufferMapContext {
    GstVideoFrame frame;
};

bool gva_buffer_map(GstBuffer *buffer, InferenceBackend::Image &image, BufferMapContext &mapContext, GstVideoInfo *info,
                    InferenceBackend::MemoryType memoryType, GstMapFlags mapFlags);

void gva_buffer_unmap(GstBuffer *buffer, InferenceBackend::Image &image, BufferMapContext &mapContext);
