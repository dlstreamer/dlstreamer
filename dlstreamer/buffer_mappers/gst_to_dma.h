/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include "dlstreamer/dma/buffer.h"
#include "dlstreamer/gst/buffer.h"
#include <gst/allocators/allocators.h>

namespace dlstreamer {

class BufferMapperGSTToDMA : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto gst_src_buffer = std::dynamic_pointer_cast<GSTBuffer>(src_buffer);
        if (!gst_src_buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to GSTBuffer");
        return map(gst_src_buffer, mode);
    }
    DMABufferPtr map(GSTBufferPtr src_buffer, AccessMode /*mode*/) {
        GstMemory *mem = gst_buffer_peek_memory(src_buffer->gst_buffer(), 0);
        if (!mem)
            throw std::runtime_error("Failed to get GstBuffer memory");

        int dma_fd = gst_dmabuf_memory_get_fd(mem);
        if (dma_fd < 0)
            throw std::runtime_error("Failed to import DMA buffer FD");

        auto dst = new DMABuffer(dma_fd, 0, src_buffer->info());
        auto deleter = [src_buffer](DMABuffer *dst) { delete dst; };
        return DMABufferPtr(dst, deleter);
    }
};

} // namespace dlstreamer
