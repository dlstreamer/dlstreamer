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

namespace dlstreamer {

class BufferMapperOpenCLToCPU : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<OpenCLBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to OpenCLBuffer");
        return map(buffer, mode);
    }

    CPUBufferPtr map(OpenCLBufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();
        std::vector<void *> data(info->planes.size());
        for (size_t i = 0; i < data.size(); i++) {
            // TODO
            // cl_mem mem = buffer->clmem(i);
            // EnqueueMapBuffer()
        }
        return CPUBufferPtr(new CPUBuffer(info, data), [buffer](CPUBuffer *dst) { delete dst; });
    }
};

} // namespace dlstreamer
