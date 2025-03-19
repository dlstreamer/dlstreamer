/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/cpu/tensor.h"
#include "dlstreamer/cpu/utils.h"

namespace dlstreamer {

static inline TensorInfo squeeze_tensor_info(const TensorInfo &info) {
    TensorInfo info2 = info;
    while (info2.shape.size() && info2.shape[0] == 1) {
        info2.shape.erase(info2.shape.begin());
        info2.stride.erase(info2.stride.begin());
    }
    return info2;
}

// Parameter 'slice' specifies offset and size over all dimensions or subset of dimensions from beginning
static inline TensorPtr get_tensor_slice(TensorPtr tensor, const std::vector<std::pair<size_t, size_t>> &slice,
                                         bool squeeze = false) {
    auto &info = tensor->info();
    TensorInfo info2({}, info.dtype, info.stride);
    size_t offset = 0;
    for (size_t i = 0; i < info.shape.size(); i++) {
        if (i < slice.size() && slice[i].second) {
            info2.shape.push_back(slice[i].second);
            offset += slice[i].first * info.stride[i];
        } else {
            info2.shape.push_back(info.shape[i]);
        }
    }
    void *data = static_cast<uint8_t *>(tensor->data()) + offset;
    auto tensor2 = std::make_shared<CPUTensor>(squeeze ? squeeze_tensor_info(info2) : info2, data);
    tensor2->set_parent(tensor);
    return tensor2;
}

} // namespace dlstreamer
