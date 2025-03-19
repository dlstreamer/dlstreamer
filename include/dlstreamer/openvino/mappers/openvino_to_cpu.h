/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/openvino/frame.h"

namespace dlstreamer {

class MemoryMapperOpenVINOToCPU : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        if (src->info().size()) {
            return src; // we can return 'src' as src->data() is blocking call waiting for inference completion
        } else { // if partial shape (0 in dimensions), update TensorInfo from ov::Tensor after inference completed
            ov::Tensor ov_t = ptr_cast<OpenVINOTensor>(src)->ov_tensor();
            auto data = src->data(); // waits for inference completion
            auto info = ov_tensor_to_tensor_info(ov_t);
            DLS_CHECK(info.size());
            auto cpu_tensor = std::make_shared<CPUTensor>(info, data);
            cpu_tensor->set_parent(src);
            return cpu_tensor;
        }
    }

    FramePtr map(FramePtr src, AccessMode mode) override {
        auto src_ov = ptr_cast<OpenVINOFrame>(src);
        {
            // ITT_TASK("Wait infer request");
            src_ov->wait();
        }
        return BaseMemoryMapper::map(src, mode);
    }
};

} // namespace dlstreamer
