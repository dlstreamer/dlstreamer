/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_base.h"
#include "dlstreamer/opencl/context.h"

#ifdef DLS_HAVE_OPENCL
#include <CL/cl.h>
#else
struct _cl_mem;
using cl_mem = _cl_mem *;
#endif

namespace dlstreamer {

class OpenCLBuffer : public BufferBase {
  public:
    static constexpr auto cl_mem_id = "cl_mem"; // (cl_mem)

    OpenCLBuffer(BufferInfoCPtr info, ContextPtr context, std::vector<cl_mem> mem)
        : BufferBase(BufferType::OPENCL_BUFFER, info, context) {
        if (mem.size() != info->planes.size())
            throw std::runtime_error("Mismatch between OpenCL buffers vector size and number planes");
        for (size_t i = 0; i < _info->planes.size(); i++) {
            set_handle(cl_mem_id, i, reinterpret_cast<handle_t>(mem[i]));
        }
    }
    cl_mem clmem(size_t plane_index) const {
        return reinterpret_cast<cl_mem>(handle(cl_mem_id, plane_index));
    }
    std::vector<std::string> keys() const override {
        return {cl_mem_id};
    }
};

#ifdef DLS_HAVE_OPENCL

class OpenCLBufferRefCounted : public OpenCLBuffer {
  public:
    OpenCLBufferRefCounted(BufferInfoCPtr info, ContextPtr context, std::vector<cl_mem> mem)
        : OpenCLBuffer(info, context, mem) {
        for (size_t i = 0; i < _info->planes.size(); i++) {
            clRetainMemObject(clmem(i));
        }
    }
    OpenCLBufferRefCounted(BufferInfoCPtr info, ContextPtr context)
        : OpenCLBuffer(info, context, create_buffers(info, context)) {
    }
    ~OpenCLBufferRefCounted() {
        for (size_t i = 0; i < _info->planes.size(); i++) {
            clReleaseMemObject(clmem(i));
        }
    }

  private:
    std::vector<cl_mem> create_buffers(BufferInfoCPtr info, ContextPtr context) {
        std::vector<cl_mem> mems;
        cl_context clcontext = static_cast<cl_context>(context->handle(OpenCLContext::cl_context_id));
        for (size_t i = 0; i < info->planes.size(); i++) {
            cl_int errcode = 0;
            cl_mem clmem = clCreateBuffer(clcontext, 0, info->planes[0].size(), 0, &errcode);
            if (!clmem || errcode)
                throw std::runtime_error("Error creating OpenCL buffer " + std::to_string(errcode));
            mems.push_back(clmem);
        }
        return mems;
    }
};

#endif

using OpenCLBufferPtr = std::shared_ptr<OpenCLBuffer>;

} // namespace dlstreamer
