/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/tensor.h"

#ifndef CL_VERSION_1_0
extern "C" {
typedef struct _cl_mem *cl_mem;
}
#endif

namespace dlstreamer {

class OpenCLTensor : public BaseTensor {
  public:
    struct key {
        static constexpr auto cl_mem = "cl_mem";                // (cl_mem)
        static constexpr auto offset = BaseTensor::key::offset; // (size_t)
    };

    OpenCLTensor(const TensorInfo &info, ContextPtr context, cl_mem mem)
        : BaseTensor(MemoryType::OpenCL, info, key::cl_mem, context) {
        set_handle(key::cl_mem, reinterpret_cast<handle_t>(mem));
    }

    cl_mem clmem() const {
        return reinterpret_cast<cl_mem>(handle(key::cl_mem));
    }

    operator cl_mem() const {
        return clmem();
    }

    size_t offset() const {
        return handle(key::offset, 0);
    }
};

using OpenCLTensorPtr = std::shared_ptr<OpenCLTensor>;

} // namespace dlstreamer
