/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "dlstreamer/buffer_mapper.h"
#include "dlstreamer/opencv/buffer.h"

namespace dlstreamer {

class BufferMapperCPUToOpenCV : public BufferMapper {
  public:
    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override {
        auto buffer = std::dynamic_pointer_cast<CPUBuffer>(src_buffer);
        if (!buffer)
            throw std::runtime_error("Failed to dynamically cast Buffer to CPUBuffer");
        return map(buffer, mode);
    }

    OpenCVBufferPtr map(CPUBufferPtr buffer, AccessMode /*mode*/) {
        auto info = buffer->info();
        std::vector<cv::Mat> mats;
        for (size_t i = 0; i < info->planes.size(); i++) {
            auto &plane = info->planes[i];
            int dtype = CV_MAKETYPE(data_type_to_opencv(plane.type), plane.channels());
            std::vector<int> shape(plane.shape.begin(), plane.shape.end());
            cv::Mat mat(shape, dtype, buffer->data(i), plane.stride.data());
            mats.push_back(mat);
        }
        return {new OpenCVBuffer(mats, info), [buffer](OpenCVBuffer *dst) { delete dst; }};
    }
};

} // namespace dlstreamer
