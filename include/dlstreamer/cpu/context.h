/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/context.h"

namespace dlstreamer {

class CPUContext : public BaseContext {
  public:
    CPUContext() : BaseContext(MemoryType::CPU) {
    }
};

using CPUContextPtr = std::shared_ptr<CPUContext>;

} // namespace dlstreamer
