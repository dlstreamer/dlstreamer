/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include <opencv2/imgproc.hpp>

namespace dlstreamer {

static inline int data_type_to_opencv(DataType type) {
    switch (type) {
    case DataType::UInt8:
        return CV_8U;
    case DataType::Int32:
        return CV_32S;
    case DataType::Int64:
        throw std::runtime_error("Int64 not supported in cv::Mat");
    case DataType::Float32:
        return CV_32F;
    }
    throw std::runtime_error("Unsupported data type");
}

} // namespace dlstreamer
