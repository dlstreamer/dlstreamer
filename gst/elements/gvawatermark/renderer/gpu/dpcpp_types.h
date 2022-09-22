/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "color_converter.h"
#include "render_prim.h"

namespace gpu {
namespace dpcpp {
// Structures in render_prim.h such as Line, Rect etc use cv::Scalar_<double> for Color field.
// But some GPU platforms don't support `double` format.
// We attached u_int8_t Color to primitives to avoid the problem.
using Rect = std::pair<render::Rect, Color>;
using Circle = std::pair<render::Circle, Color>;

struct Line {
    int x0;
    int x1;
    int y0;
    int y1;
    Color color;
    bool steep;
};

struct RasterText {
    uint8_t *bitmap; // gpu located
    int x;
    int y;
    int w;
    int h;
};

using Text = std::pair<gpu::dpcpp::RasterText, Color>;

} // namespace dpcpp
} // namespace gpu
