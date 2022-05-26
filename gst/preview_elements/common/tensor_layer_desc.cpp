/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_layer_desc.hpp"

#include <safe_arithmetic.hpp>

TensorLayerDesc TensorLayerDesc::FromIeDesc(const InferenceEngine::TensorDesc &desc, const std::string &layer_name) {
    TensorLayerDesc result;
    result.precision = desc.getPrecision();
    result.layout = desc.getLayout();
    result.dims = desc.getDims();
    result.size =
        std::accumulate(desc.getDims().begin(), desc.getDims().end(), result.precision.size(), safe_mul<size_t>);
    result.layer_name = layer_name;
    return result;
}

InferenceEngine::TensorDesc TensorLayerDesc::ToIeDesc() const {
    return InferenceEngine::TensorDesc(precision, dims, layout);
}
