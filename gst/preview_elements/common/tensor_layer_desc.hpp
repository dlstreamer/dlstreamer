/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "capabilities/types.hpp"

#include <ie_core.hpp>

struct TensorLayerDesc {
    Precision precision = Precision::UNSPECIFIED;
    Layout layout = Layout::ANY;
    std::vector<size_t> dims;
    std::string layer_name;
    size_t size;

    explicit operator bool() const {
        return !dims.empty();
    }

    static TensorLayerDesc FromIeDesc(const InferenceEngine::TensorDesc &desc, const std::string &layer_name);
    InferenceEngine::TensorDesc ToIeDesc() const;
};
