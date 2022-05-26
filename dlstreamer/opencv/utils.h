/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer.h"
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

namespace dlstreamer {

static inline int data_type_to_opencv(DataType type) {
    switch (type) {
    case DataType::U8:
        return CV_8U;
    case DataType::I32:
        return CV_32S;
    case DataType::FP32:
        return CV_32F;
    }
    throw std::runtime_error("Unsupported data type");
}

} // namespace dlstreamer
