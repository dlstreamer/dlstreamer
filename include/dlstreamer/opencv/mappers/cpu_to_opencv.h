/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/opencv/tensor.h"

namespace dlstreamer {

class MemoryMapperCPUToOpenCV : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    MemoryMapperCPUToOpenCV() : BaseMemoryMapper(nullptr, nullptr) {
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        cv::Mat mat;
        auto &info = src->info();
        std::vector<int> shape(info.shape.begin(), info.shape.end());
        std::vector<size_t> stride = info.stride;
        while (shape.size() > 2 && shape[0] == 1) {
            shape.erase(shape.begin());
            stride.erase(stride.begin());
        }
        ImageLayout layout(info.shape);
        if (layout == ImageLayout::HWC || layout == ImageLayout::NHWC) {
            shape.pop_back();
            stride.pop_back();
            int dtype = CV_MAKETYPE(data_type_to_opencv(info.dtype), ImageInfo(info).channels());
            mat = cv::Mat(shape, dtype, src->data(), stride.data());
        } else {
            int dtype = CV_MAKETYPE(data_type_to_opencv(info.dtype), 1);
            mat = cv::Mat(shape, dtype, src->data(), stride.data());
        }
        auto ret = std::make_shared<OpenCVTensor>(mat, info);
        ret->set_parent(src);
        return ret;
    }
};

} // namespace dlstreamer
