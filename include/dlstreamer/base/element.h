/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/element.h"

#include <mutex>

namespace dlstreamer {

template <class T>
class BaseElement : public T {
  public:
    bool init() override {
        std::call_once(_init_flag, [this] { _init_result = init_once(); });
        return _init_result;
    }

    ContextPtr get_context(MemoryType /*memory_type*/) noexcept override {
        return nullptr;
    }

  protected:
    virtual bool init_once() {
        return true;
    }

  private:
    std::once_flag _init_flag;
    bool _init_result;
};

} // namespace dlstreamer
