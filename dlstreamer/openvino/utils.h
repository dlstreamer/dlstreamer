/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer.h"
#include <ie_compound_blob.h>
#include <inference_engine.hpp>
#ifdef HAVE_OPENVINO2
#include <openvino/openvino.hpp>
#endif

namespace dlstreamer {

namespace IE = InferenceEngine;

static inline DataType data_type_from_openvino(IE::Precision precision) {
    switch (precision) {
    case IE::Precision::U8:
        return DataType::U8;
    case IE::Precision::FP32:
        return DataType::FP32;
    case IE::Precision::I32:
        return DataType::I32;
    default:
        throw std::runtime_error("Unsupported IE::Precision " + std::to_string(precision));
    }
}

#ifdef HAVE_OPENVINO2
static inline DataType data_type_from_openvino(ov::element::Type element) {
    switch (element) {
    case ov::element::Type_t::u8:
        return DataType::U8;
    case ov::element::Type_t::f32:
        return DataType::FP32;
    case ov::element::Type_t::i32:
        return DataType::I32;
    default:
        break;
    }
    throw std::runtime_error("Unsupported OV element type: " + element.get_type_name());
}
#endif

static inline IE::Precision data_type_to_openvino(DataType type) {
    switch (type) {
    case DataType::U8:
        return IE::Precision::U8;
    case DataType::FP32:
        return IE::Precision::FP32;
    case DataType::I32:
        return IE::Precision::I32;
    default:
        throw std::runtime_error("Unsupported DataType");
    }
}

static inline Layout layout_from_openvino(IE::Layout layout) {
    switch (layout) {
    case IE::Layout::CHW:
        return Layout::CHW;
    case IE::Layout::HWC:
        return Layout::HWC;
    case IE::Layout::NCHW:
        return Layout::NCHW;
    case IE::Layout::NHWC:
        return Layout::NHWC;
    // case IE::Layout::ANY: return Layout::ANY;
    default:
        throw std::runtime_error("Unsupported IE::Layout " + std::to_string(layout));
    }
}

static inline IE::Layout layout_to_openvino(Layout layout) {
    switch (layout) {
    case Layout::CHW:
        return IE::Layout::CHW;
    case Layout::HWC:
        return IE::Layout::HWC;
    case Layout::NCHW:
        return IE::Layout::NCHW;
    case Layout::NHWC:
        return IE::Layout::NHWC;
    // case Layout::ANY: return IE::Layout::ANY;
    default:
        throw std::runtime_error("Unsupported Layout " + std::to_string(layout));
    }
}

static inline PlaneInfo tensor_desc_to_plane_info(const IE::TensorDesc &desc, std::string layer_name = std::string()) {
    // plane.layout = layout_from_openvino(desc.getLayout());
    return PlaneInfo(desc.getDims(), data_type_from_openvino(desc.getPrecision()), layer_name);
}

static inline IE::TensorDesc plane_info_to_tensor_desc(const PlaneInfo &info) {
    return {data_type_to_openvino(info.type), info.shape, layout_to_openvino(info.layout)};
}

} // namespace dlstreamer
