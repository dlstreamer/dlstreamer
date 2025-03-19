/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include <utility>

template <class Fn>
class ScopeGuard {
  public:
    ScopeGuard(Fn &&func) : my_func(std::forward<Fn>(func)) {
    }
    ~ScopeGuard() {
        if (armed)
            my_func();
    }

    ScopeGuard(ScopeGuard &&other) : my_func(std::move(other.my_func)), armed(other.armed) {
        other.armed = false;
    }

    void disable() {
        armed = false;
    }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

  private:
    Fn my_func;
    bool armed = true;
};

template <class Fn>
ScopeGuard<Fn> makeScopeGuard(Fn &&rollback_fn) {
    return ScopeGuard<Fn>(std::forward<Fn>(rollback_fn));
}
