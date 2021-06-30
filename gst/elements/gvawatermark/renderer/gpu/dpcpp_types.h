/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "color_converter.h"
#include <opencv2/gapi.hpp>
#include <opencv2/gapi/render/render.hpp>
#include <opencv2/opencv.hpp>

namespace gpu {
namespace dpcpp {
// gapi::Primitives such as Line, Rect etc uses cv::Scalar_<double> for Color field.
// But some GPU platforms does not support `double` format.
// We attached u_int8_t Color to primitives to avoid the problem.
using Rect = std::pair<cv::gapi::wip::draw::Rect, Color>;
using Circle = std::pair<cv::gapi::wip::draw::Circle, Color>;
using Line = std::pair<cv::gapi::wip::draw::Line, Color>;

struct RasterText {
    uint8_t *bitmap; // gpu located
    int x;
    int y;
    int w;
    int h;
};

using Text = std::pair<gpu::dpcpp::RasterText, Color>;

struct MaskedPixel {
    cv::Scalar_<u_int8_t> ch;
    bool colored; // Indicates should the exact pixel be colored or not.
};

struct SubsampligParams {
    uint8_t J;
    uint8_t a;
    uint8_t b;
};
} // namespace dpcpp
} // namespace gpu