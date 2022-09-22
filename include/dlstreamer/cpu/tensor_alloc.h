/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/tensor.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/cpu/tensor.h"

namespace dlstreamer {

class CPUTensorAlloc final : public CPUTensor {
  public:
    CPUTensorAlloc(const TensorInfo &info) : CPUTensor(info, malloc(info.nbytes())) {
    }

    ~CPUTensorAlloc() {
        if (_data) {
            free(_data);
            _data = nullptr;
        }
    }
};

} // namespace dlstreamer
