/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/opencl/tensor.h"

#ifndef CL_EXTERNAL_MEMORY_HANDLE_INTEL
#define CL_EXTERNAL_MEMORY_HANDLE_INTEL 0x10050
#endif

namespace dlstreamer {

class MemoryMapperOpenCLToDMA : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto src_cl = ptr_cast<OpenCLTensor>(src);
        int64_t dma_fd = -1;
        auto err = clGetMemObjectInfo(*src_cl, CL_EXTERNAL_MEMORY_HANDLE_INTEL, sizeof(dma_fd), &dma_fd, nullptr);
        if (err || dma_fd <= 0)
            throw std::runtime_error("Error getting DMA-FD from OpenCL memory: " + std::to_string(err));

        auto ret = std::make_shared<DMATensor>(dma_fd, 0, src->info());
        ret->set_parent(src);
        return ret;
    }
};

} // namespace dlstreamer
