/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer/base/frame.h"
#include "dlstreamer/cpu/context.h"
#include "dlstreamer/cpu/tensor_alloc.h"

namespace dlstreamer {

class CPUFrameAlloc final : public BaseFrame {
  public:
    CPUFrameAlloc(const FrameInfo &info) : BaseFrame(MediaType::Tensors, 0, create_tensors(info)) {
    }

  private:
    TensorVector create_tensors(const FrameInfo &finfo) {
        TensorVector tensors;
        for (auto &info : finfo.tensors) {
            tensors.push_back(std::make_shared<CPUTensorAlloc>(info));
        }
        return tensors;
    }
};

} // namespace dlstreamer
