/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <inference_backend/image.h>

#include <gst/gstbuffer.h>
#include <gst/video/video-info.h>

#include <memory>

// FWD
using VaApiDisplayPtr = std::shared_ptr<void>;

class BufferMapper {
  public:
    using Ptr = std::shared_ptr<BufferMapper>;

    BufferMapper() = default;
    virtual ~BufferMapper() = default;

    // Returns target memory type of mapper
    virtual InferenceBackend::MemoryType memoryType() const = 0;

    virtual InferenceBackend::Image map(GstBuffer *buffer, GstMapFlags flags) = 0;
    virtual void unmap(InferenceBackend::Image &image) = 0;
};

class VideoBufferMapper : public BufferMapper {
  public:
    VideoBufferMapper(const GstVideoInfo &info);
    ~VideoBufferMapper();

  protected:
    static void fillImageFromVideoInfo_(const GstVideoInfo *vinfo, InferenceBackend::Image &image);

    GstVideoInfo *vinfo_;
    InferenceBackend::Image image_boilerplate_;
};

class BufferMapperFactory {
  public:
    static std::unique_ptr<BufferMapper> createMapper(InferenceBackend::MemoryType memory_type,
                                                      const GstVideoInfo *info);
    static std::unique_ptr<BufferMapper> createMapper(InferenceBackend::MemoryType memory_type,
                                                      const GstVideoInfo *info, VaApiDisplayPtr va_dpy);

    BufferMapperFactory() = delete;
};
