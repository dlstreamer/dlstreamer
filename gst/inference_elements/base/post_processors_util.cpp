/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include <cmath>
using namespace std;
void softMax(float *x, size_t size) {
    if (x == nullptr || size == 0)
        return;
    const float t0 = -100.0f;
    float max = x[0], min = x[0];
    for (size_t i = 1; i < size; i++) {
        if (min > x[i])
            min = x[i];
        if (max < x[i])
            max = x[i];
    }

    const float min_t0 = min * t0;
    for (size_t i = 0; i < size; i++) {
        x[i] -= max;
        if (min < t0) {
            x[i] /= min_t0;
        }
    }

    // normalize as probabilities
    float expsum = 0;
    for (size_t i = 0; i < size; i++) {
        x[i] = std::exp(x[i]);
        expsum += x[i];
    }
    for (size_t i = 0; i < size; i++) {
        x[i] /= expsum;
    }
}

float sigmoid(float x) {
    return 1.f / (1.f + std::exp(-x));
}
