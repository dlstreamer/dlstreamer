/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_base.h"
#include "dlstreamer/opencv/utils.h"

namespace dlstreamer {

class OpenCVBuffer : public BufferBase {
  public:
    OpenCVBuffer(const std::vector<cv::Mat> &mats, BufferInfoCPtr info)
        : BufferBase(BufferType::OPENCV, mats_to_buffer_info(mats, info)), _mats(mats) {
    }

    cv::Mat mat(int index = 0) {
        return _mats[index];
    }

    void *data(size_t index = 0) const override {
        return _mats.at(index).data;
    }

  protected:
    std::vector<cv::Mat> _mats;

    BufferInfoCPtr mats_to_buffer_info(const std::vector<cv::Mat> &mats, BufferInfoCPtr info0) {
        if (info0)
            return info0;
        auto info = std::make_shared<BufferInfo>();
        for (const auto &mat : mats) {
            DataType data_type = DataType::U8; // TODO
            std::vector<int> shape(mat.size.p, mat.size.p + mat.size.dims());
            PlaneInfo plane_info(std::vector<size_t>(shape.begin(), shape.end()), data_type);
            info->planes.push_back(plane_info);
        }
        return info;
    }
};

using OpenCVBufferPtr = std::shared_ptr<OpenCVBuffer>;

} // namespace dlstreamer
