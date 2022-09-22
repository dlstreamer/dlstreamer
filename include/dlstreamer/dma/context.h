/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"

namespace dlstreamer {

class DMAContext;
using DMAContextPtr = std::shared_ptr<DMAContext>;

class DMAContext : public BaseContext {
  public:
    static inline DMAContextPtr create() {
        return std::make_shared<DMAContext>();
    }

    DMAContext() : BaseContext(MemoryType::DMA) {
    }

    static inline DMAContextPtr create(ContextPtr /*another_context*/) {
        return std::make_shared<DMAContext>();
    }
};

} // namespace dlstreamer
