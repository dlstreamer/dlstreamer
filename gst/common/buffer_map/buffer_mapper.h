/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"

#include <gst/gstbuffer.h>
#include <gst/video/video-info.h>

#include <memory>

class BufferMapper {
  public:
    using Ptr = std::shared_ptr<BufferMapper>;

    BufferMapper() = default;
    virtual ~BufferMapper() = default;

    virtual InferenceBackend::Image map(GstBuffer *buffer, GstVideoInfo *info, GstMapFlags flag) = 0;
    virtual void unmap() = 0;
};
