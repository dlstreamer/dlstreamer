/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/opencl/tensor.h"
#include "dlstreamer/utils.h"

#include <CL/cl.h>

#ifndef CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR
#define CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR 0x2067
#endif

namespace dlstreamer {

extern "C" {
// clCreateBufferWithProperties introduced in OpenCL 3.0
// clCreateBufferWithPropertiesINTEL has same signature and same functionality
typedef CL_API_ENTRY cl_mem(CL_API_CALL *clCreateBufferWithPropertiesINTEL_fn)(cl_context context,
                                                                               const cl_bitfield *properties,
                                                                               cl_mem_flags flags, size_t size,
                                                                               void *host_ptr, cl_int *errcode_ret);
}

class MemoryMapperDMAToOpenCL : public BaseMemoryMapper {
  public:
    MemoryMapperDMAToOpenCL(const ContextPtr &input_context, const ContextPtr &output_context)
        : BaseMemoryMapper(input_context, output_context) {
        DLS_CHECK(_cl_ctx = cl_context(output_context->handle(BaseContext::key::cl_context)));

        cl_device_id device = 0;
        DLS_CHECK_GE0(clGetContextInfo(_cl_ctx, CL_CONTEXT_DEVICES, sizeof(device), &device, NULL));
        cl_platform_id platform = 0;
        DLS_CHECK_GE0(clGetDeviceInfo(device, CL_DEVICE_PLATFORM, sizeof(platform), &platform, NULL));
        auto fn = clGetExtensionFunctionAddressForPlatform(platform, "clCreateBufferWithPropertiesINTEL");
        DLS_CHECK(_clCreateBufferWithPropertiesINTEL = reinterpret_cast<clCreateBufferWithPropertiesINTEL_fn>(fn));
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        uint64_t dma_fd = ptr_cast<DMATensor>(src)->dma_fd();

        cl_bitfield mem_properties[] = {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, dma_fd, 0};
        cl_int errcode = -1;
        size_t size = src->info().size();
        cl_mem mem = _clCreateBufferWithPropertiesINTEL(_cl_ctx, mem_properties, 0, size, NULL, &errcode);
        DLS_CHECK_GE0(errcode)
        DLS_CHECK(mem)
        // printf("clCreateBufferWithProperties: %d -> %p\n", (int)dma_fd, mem);

        auto dst = std::make_shared<OpenCLTensor>(src->info(), _output_context, mem);
        dst->set_parent(src);
        return dst;
    }

  private:
    cl_context _cl_ctx;
    clCreateBufferWithPropertiesINTEL_fn _clCreateBufferWithPropertiesINTEL = nullptr;
};

} // namespace dlstreamer
