/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/opencl/tensor.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

class MemoryMapperOpenCLToCPU : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto cl_src = ptr_cast<OpenCLTensor>(src);
        auto &info = cl_src->info();
        void *data = nullptr;
        // TODO
        // cl_mem mem = cl_src->clmem();
        // EnqueueMapBuffer()
        auto ret = std::make_shared<CPUTensor>(info, data);
        ret->set_parent(src);
        return ret;
    }
};

} // namespace dlstreamer
