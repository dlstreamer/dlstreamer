/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dpcpp_types.h"

#include <level_zero/ze_api.h>

#include <CL/sycl.hpp>
#include <CL/sycl/backend.hpp>
#include <CL/sycl/backend/level_zero.hpp>
#include <drm/drm_fourcc.h>

#include <utility>

namespace gpu {
namespace dpcpp {

sycl::event renderRectangles(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask,
                             const gpu::dpcpp::Rect *rectangles, size_t size, size_t max_length);

sycl::event renderTexts(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask, const gpu::dpcpp::Text *texts,
                        size_t size, size_t max_height, size_t max_width);

sycl::event renderCircles(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask,
                          const gpu::dpcpp::Circle *circles, size_t size, size_t max_radius);

sycl::event renderLines(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask, const gpu::dpcpp::Line *lines,
                        size_t size, size_t thick);

sycl::event mix(sycl::queue queue, gpu::dpcpp::MaskedPixel *mask, cv::Mat &image_plane, int plane_index,
                gpu::dpcpp::SubsampligParams subsampling, uint64_t drm_format_modifier);

} // namespace dpcpp
} // namespace gpu
