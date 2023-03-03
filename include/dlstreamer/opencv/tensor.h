/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/opencv/utils.h"

namespace dlstreamer {

namespace tensor::key {
static constexpr auto cv_mat = "cv_mat"; // (cv::Mat*)
};

class OpenCVTensor : public BaseTensor {
  public:
    OpenCVTensor(const cv::Mat &mat, const TensorInfo &info)
        : BaseTensor(MemoryType::OpenCV, info, tensor::key::cv_mat), _mat(mat) {
        set_handle(tensor::key::cv_mat, reinterpret_cast<handle_t>(&_mat));
    }

    OpenCVTensor(const cv::Mat &mat)
        : BaseTensor(MemoryType::OpenCV, mat_to_tensor_info(mat), tensor::key::cv_mat), _mat(mat) {
        set_handle(tensor::key::cv_mat, reinterpret_cast<handle_t>(&_mat));
    }

    operator cv::Mat() const {
        return _mat;
    }

    cv::Mat cv_mat() const {
        return _mat;
    }

    void *data() const override {
        return _mat.data;
    }

  protected:
    cv::Mat _mat;

    TensorInfo mat_to_tensor_info(const cv::Mat &mat) {
        DataType data_type = DataType::UInt8; // TODO
        std::vector<int> shape(mat.size.p, mat.size.p + mat.size.dims());
        return TensorInfo(std::vector<size_t>(shape.begin(), shape.end()), data_type);
    }
};

using OpenCVTensorPtr = std::shared_ptr<OpenCVTensor>;

} // namespace dlstreamer
