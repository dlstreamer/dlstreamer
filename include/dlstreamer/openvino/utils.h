/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include <openvino/openvino.hpp>

namespace dlstreamer {

static inline DataType data_type_from_openvino(ov::element::Type element) {
    switch (element) {
    case ov::element::Type_t::u8:
        return DataType::UInt8;
    case ov::element::Type_t::f32:
        return DataType::Float32;
    case ov::element::Type_t::i32:
        return DataType::Int32;
    case ov::element::Type_t::i64:
        return DataType::Int64;
    default:
        break;
    }
    throw std::runtime_error("Unsupported OV element type: " + element.get_type_name());
}

static inline ov::element::Type data_type_to_openvino(DataType type) {
    switch (type) {
    case DataType::UInt8:
        return ov::element::Type_t::u8;
    case DataType::Float32:
        return ov::element::Type_t::f32;
    case DataType::Int32:
        return ov::element::Type_t::i32;
    case DataType::Int64:
        return ov::element::Type_t::i64;
    default:
        throw std::runtime_error("Unsupported DataType");
    }
}

static inline TensorInfo ov_tensor_to_tensor_info(ov::Tensor tensor) {
    return TensorInfo(tensor.get_shape(), data_type_from_openvino(tensor.get_element_type()), tensor.get_strides());
}

} // namespace dlstreamer
