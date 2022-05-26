/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/dma/buffer.h"
#include "dlstreamer/opencl/buffer.h"

#ifndef CL_EXTERNAL_MEMORY_HANDLE_INTEL
#define CL_EXTERNAL_MEMORY_HANDLE_INTEL 0x10050
#endif

namespace dlstreamer {

#ifdef DLS_HAVE_OPENCL

class BufferMapperOpenCLToDMA : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<OpenCLBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to OpenCLBuffer");
        return map(buffer, mode);
    }

    DMABufferPtr map(OpenCLBufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();
        if (info->planes.size() != 1)
            throw std::runtime_error("BufferMapperOpenCLToDMA supports single-plane data only");
        cl_mem mem = buffer->clmem(0);
        int64_t dma_fd = -1;
        auto err = clGetMemObjectInfo(mem, CL_EXTERNAL_MEMORY_HANDLE_INTEL, sizeof(dma_fd), &dma_fd, nullptr);
        if (err || dma_fd <= 0)
            throw std::runtime_error("Error getting DMA-FD from OpenCL memory: " + std::to_string(err));
        return DMABufferPtr(new DMABuffer(dma_fd, 0, info), [buffer](DMABuffer *dst) { delete dst; });
    }
};

#endif

} // namespace dlstreamer
