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
        // std::call_once can't be used here due to bug in gcc:
        //   In case of exception second call to std::call_once will hang
        //   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66146
        // It's not expected to be a concurrent call, so it can be replaced with a simple flag.
        if (!_init_flag) {
            _init_result = init_once();
            _init_flag = true;
        }
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
    bool _init_flag = false;
    bool _init_result = false;
};

} // namespace dlstreamer
