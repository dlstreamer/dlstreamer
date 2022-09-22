/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/sycl/context.h"
#include "dlstreamer/utils.h"

namespace dlstreamer {

class MemoryMapperSYCLUSMToCPU : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto usm_type = static_cast<sycl::usm::alloc>(src->handle("usm_type"));
        DLS_CHECK(usm_type == sycl::usm::alloc::host || usm_type == sycl::usm::alloc::shared)
        return src;
    }
};

} // namespace dlstreamer
