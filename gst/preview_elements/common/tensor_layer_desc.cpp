/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tensor_layer_desc.hpp"

#include <safe_arithmetic.hpp>
#include <utils.h>

size_t count_tensor_size(const InferenceEngine::TensorDesc &tensor_desc) {
    size_t size = 1;
    for (auto dim : tensor_desc.getDims())
        size = safe_mul(dim, size);
    size = safe_mul(size, tensor_desc.getPrecision().size());
    return size;
}

TensorLayerDesc TensorLayerDesc::FromIeDesc(const InferenceEngine::TensorDesc &desc, const std::string &layer_name) {
    TensorLayerDesc result;
    result.precision = desc.getPrecision();
    result.layout = desc.getLayout();
    result.dims = desc.getDims();
    result.size = count_tensor_size(desc);
    result.layer_name = layer_name;
    return result;
}

InferenceEngine::TensorDesc TensorLayerDesc::ToIeDesc() const {
    return InferenceEngine::TensorDesc(precision, dims, layout);
}
