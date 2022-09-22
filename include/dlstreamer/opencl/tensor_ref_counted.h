/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/opencl/tensor.h"

namespace dlstreamer {

class OpenCLTensorRefCounted : public OpenCLTensor {
  public:
    OpenCLTensorRefCounted(const TensorInfo &info, ContextPtr context, cl_mem mem) : OpenCLTensor(info, context, mem) {
        clRetainMemObject(mem);
    }
    OpenCLTensorRefCounted(const TensorInfo &info, ContextPtr context)
        : OpenCLTensor(info, context, create_buffer(info, context)) {
    }
    ~OpenCLTensorRefCounted() {
        clReleaseMemObject(clmem());
    }

  private:
    cl_mem create_buffer(const TensorInfo &info, ContextPtr context) {
        cl_context clcontext = static_cast<cl_context>(context->handle(OpenCLContext::key::cl_context));
        cl_int errcode = 0;
        cl_mem clmem = clCreateBuffer(clcontext, 0, info.nbytes(), 0, &errcode);
        if (!clmem || errcode)
            throw std::runtime_error("Error creating OpenCL buffer " + std::to_string(errcode));
        return clmem;
    }
};

} // namespace dlstreamer
