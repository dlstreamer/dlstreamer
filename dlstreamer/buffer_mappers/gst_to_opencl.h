/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/gst/allocator.h"
#include "dlstreamer/gst/buffer.h"
#include "dlstreamer/opencl/buffer.h"

namespace dlstreamer {

class BufferMapperGSTToOpenCL : public BufferMapper {
  public:
    BufferMapperGSTToOpenCL(ContextPtr opencl_context) : _opencl_context(opencl_context) {
    }

    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto gst_src_buffer = std::dynamic_pointer_cast<GSTBuffer>(src_buffer);
        if (!gst_src_buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to GSTBuffer");
        return map(gst_src_buffer, mode);
    }
    OpenCLBufferPtr map(GSTBufferPtr src_buffer, AccessMode /*mode*/) {
        GstBuffer *gst_buffer = src_buffer->gst_buffer();

        GstMapFlags map_flags = GST_MAP_NATIVE_HANDLE;
        size_t n_planes = src_buffer->info()->planes.size();
        std::vector<cl_mem> clmem(n_planes);
        for (guint i = 0; i < n_planes; ++i) {
            GstMemory *mem = gst_buffer_peek_memory(gst_buffer, i);
            GstMapInfo map_info;
            if (!mem || !gst_memory_map(mem, &map_info, map_flags))
                throw std::runtime_error("BufferMapperGSTToOpenCL: failed to map GstBuffer");
            clmem[i] = reinterpret_cast<cl_mem>(map_info.data);
        }
        auto deleter = [src_buffer](OpenCLBuffer *dst) { delete dst; };
        return OpenCLBufferPtr(new OpenCLBuffer(src_buffer->info(), _opencl_context, clmem), deleter);
    }

  protected:
    ContextPtr _opencl_context;
};

} // namespace dlstreamer
