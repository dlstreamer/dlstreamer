/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/opencv/utils.h"

namespace dlstreamer {

class OpenCVUMatTensor : public BaseTensor {
  public:
    struct key {
        static constexpr auto cv_umat = "cv_umat"; // (cv::UMat*)
    };

    OpenCVUMatTensor(const cv::UMat &umat, const TensorInfo &info)
        : BaseTensor(MemoryType::OpenCVUMat, info, key::cv_umat), _umat(umat) {
        set_handle(key::cv_umat, reinterpret_cast<handle_t>(&_umat));
    }

    OpenCVUMatTensor(const cv::UMat &umat)
        : BaseTensor(MemoryType::OpenCVUMat, umat_to_tensor_info(umat), key::cv_umat), _umat(umat) {
        set_handle(key::cv_umat, reinterpret_cast<handle_t>(&_umat));
    }

    operator cv::UMat() const {
        return _umat;
    }

    cv::UMat umat() const {
        return _umat;
    }

  protected:
    cv::UMat _umat;

    TensorInfo umat_to_tensor_info(const cv::UMat &umat) {
        DataType data_type = DataType::UInt8; // TODO
        std::vector<int> shape(umat.size.p, umat.size.p + umat.size.dims());
        return TensorInfo(std::vector<size_t>(shape.begin(), shape.end()), data_type);
    }
};

using OpenCVUMatTensorPtr = std::shared_ptr<OpenCVUMatTensor>;

} // namespace dlstreamer
