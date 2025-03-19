/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/tensor_info.h"
#include <CL/cl.h>

namespace dlstreamer {

cl_channel_order num_channels_to_opencl(int channels) {
    switch (channels) {
    case 1:
        return CL_R;
    case 2:
        return CL_RG;
    case 3:
        return CL_RGB;
    case 4:
        return CL_RGBA;
    }
    throw std::runtime_error("Unsupported number channels: " + std::to_string(channels));
}

cl_channel_type data_type_to_opencl(DataType type) {
    switch (type) {
    case DataType::UInt8:
        return CL_UNSIGNED_INT8;
    case DataType::Int32:
        return CL_SIGNED_INT32;
    case DataType::Float32:
        return CL_FLOAT;
    }
    throw std::runtime_error("Unsupported data type");
}

} // namespace dlstreamer
