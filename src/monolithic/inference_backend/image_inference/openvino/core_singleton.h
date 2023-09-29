/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <ie_core.hpp>

class IeCoreSingleton {
  private:
    IeCoreSingleton(){};
    IeCoreSingleton(const IeCoreSingleton &other) = delete;
    IeCoreSingleton &operator=(const IeCoreSingleton &other) = delete;

  public:
    static InferenceEngine::Core &Instance() {
        // thread-safe starting from C++11
        static InferenceEngine::Core core;
        return core;
    }
};
