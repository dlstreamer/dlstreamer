/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once
#include <cstddef>

void softMax(float *x, std::size_t count);
float sigmoid(float x);

template <typename T>
float dequantize(T value) {
    return (value - 221) * 0.33713474f;
}
