/*******************************************************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include <cstddef>
#include <tensor.h>

void softMax(float *x, std::size_t count);
float sigmoid(float x);

struct Dequantizer {
    double shift = 221;
    double scale = 0.33713474;

    Dequantizer() = default;

    Dequantizer(const GVA::Tensor &tensor) {
        if (tensor.has_field("dequantize_shift"))
            shift = tensor.get_double("dequantize_shift");
        if (tensor.has_field("dequantize_scale"))
            scale = tensor.get_double("dequantize_scale");
    }

    Dequantizer(GstStructure *s) {
        gst_structure_get_double(s, "dequantize_shift", &shift);
        gst_structure_get_double(s, "dequantize_scale", &scale);
    }

    template <typename T>
    float dequantize(T value) {
        return (value - shift) * scale;
    }
};
