/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/tensor.h"

namespace dlstreamer {

class CPUTensor : public BaseTensor {
  public:
    struct key {
        static constexpr auto data = "data"; // void*
    };

    CPUTensor(const TensorInfo &info, void *data) : BaseTensor(MemoryType::CPU, info, key::data), _data(data) {
        set_handle(key::data, reinterpret_cast<handle_t>(data));
    }

    void *data() const override {
        return _data;
    }

  protected:
    void *_data;
};

using CPUTensorPtr = std::shared_ptr<CPUTensor>;

} // namespace dlstreamer
