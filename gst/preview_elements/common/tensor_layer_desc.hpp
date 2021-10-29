/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "capabilities/types.hpp"

#include <ie_core.hpp>

#include <gst/gst.h>

struct TensorLayerDesc {
    InferenceEngine::Precision precision = InferenceEngine::Precision::UNSPECIFIED;
    InferenceEngine::Layout layout = InferenceEngine::Layout::ANY;
    std::vector<size_t> dims;
    std::string layer_name;
    size_t size;

    std::string layout_string() const {
        std::ostringstream stream;
        stream << layout;
        return stream.str();
    }

    explicit operator bool() {
        return !dims.empty();
    }

    static TensorLayerDesc FromIeDesc(const InferenceEngine::TensorDesc &desc, const std::string &layer_name);
    InferenceEngine::TensorDesc ToIeDesc() const;
};

size_t count_tensor_size(const InferenceEngine::TensorDesc &tensor_desc);
