/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dpcpp_draw.h"

#include <level_zero/ze_api.h>

#include <CL/sycl.hpp>
#include <CL/sycl/backend.hpp>
#include <CL/sycl/backend/level_zero.hpp>
#include <algorithm>
#include <cmath>
#include <drm/drm_fourcc.h>

// Coordinates conversion for tiled memory layout
constexpr int TILE_X_POW = 4;
constexpr int TILE_Y_POW = 5;
constexpr int TILE_X_MASK = ((1 << TILE_X_POW) - 1);
constexpr int TILE_Y_MASK = ((1 << TILE_Y_POW) - 1);
#define TILED_OFFSET(x, y, step)                                                                                       \
    ((y & ~TILE_Y_MASK) * step + ((x & ~TILE_X_MASK) << TILE_Y_POW) + ((y & TILE_Y_MASK) << TILE_X_POW) +              \
     (x & TILE_X_MASK))

namespace {

inline void setColor(const Color &src, gpu::dpcpp::MaskedPixel &dst) {
    dst.ch[0] = src[0];
    dst.ch[1] = src[1];
    dst.ch[2] = src[2];
    dst.ch[3] = src[3];
    dst.colored = true;
};
} // namespace

sycl::event gpu::dpcpp::renderRectangles(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask,
                                         const gpu::dpcpp::Rect *rectangles, size_t rectangles_size,
                                         size_t max_length) {
    sycl::event e;
    size_t local_length = 0;
    sycl::device device = queue.get_device();
    auto wgroup_size = device.get_info<sycl::info::device::max_work_group_size>();

    if (max_length <= wgroup_size) {
        local_length = max_length;
    } else {
        local_length = wgroup_size;
        max_length = (max_length / wgroup_size + 1) * wgroup_size;
    }

    sycl::range global{rectangles_size, max_length};
    sycl::range local{1, local_length};

    e = queue.parallel_for<class RenderRectangle>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
        const int k = item.get_global_id(0);
        const int i = item.get_global_id(1);
        const auto &draw_rect = rectangles[k];
        const auto thick = draw_rect.first.thick;
        const auto &rect = draw_rect.first.rect;
        const auto x = rect.x + i;
        const auto y = rect.y + i;
        if (x <= rect.x + rect.width + thick) {
            for (auto j = 0; j < thick; j++) {
                setColor(draw_rect.second, mask[x + (rect.y + j) * width]);
                setColor(draw_rect.second, mask[x + (rect.y + rect.height + thick + j) * width]);
            }
        }
        if (y <= rect.y + rect.height + thick) {
            for (auto j = 0; j < thick; j++) {
                setColor(draw_rect.second, mask[(rect.x + j) + y * width]);
                setColor(draw_rect.second, mask[(rect.x + rect.width + thick + j) + y * width]);
            }
        }
    });
    return e;
}

sycl::event gpu::dpcpp::renderLines(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask,
                                    const gpu::dpcpp::Line *lines, size_t lines_size, size_t thick) {
    sycl::event e;
    sycl::range global{lines_size, thick};
    sycl::range local{1, 1};
    e = queue.parallel_for<class RenderLine>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
        const int k = item.get_global_id(0);
        const int i = item.get_global_id(1);
        const auto &line = lines[k].first;
        auto x0 = line.pt1.x;
        auto x1 = line.pt2.x;
        auto y0 = line.pt1.y;
        auto y1 = line.pt2.y;
        auto steep = abs(y1 - y0) > abs(x1 - x0);
        if (!steep) {
            y0 += i;
            y1 += i;
        } else {
            x0 += i;
            x1 += i;
            std::swap(x0, y0);
            std::swap(x1, y1);
        }
        if (x0 > x1) {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }
        auto dx = x1 - x0;
        auto dy = abs(y1 - y0);
        auto error = dx / 2;
        int ystep = (y0 < y1) ? 1 : -1;
        int x = x0;
        int y = y0;
        int &x_c = x;
        int &y_c = y;
        if (steep) {
            x_c = y;
            y_c = x;
        }
        for (x = x0; x <= x1; x++) {
            setColor(lines[k].second, mask[x_c + y_c * width]);
            error -= dy;
            if (error < 0) {
                y += ystep;
                error += dx;
            }
        }
    });
    return e;
}

sycl::event gpu::dpcpp::renderTexts(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask,
                                    const gpu::dpcpp::Text *texts, size_t texts_size, size_t max_height,
                                    size_t max_width) {
    sycl::event e;
    sycl::device device = queue.get_device();
    auto wgroup_size = device.get_info<sycl::info::device::max_work_group_size>();
    size_t local_width = 0;
    size_t local_height = 0;

    if (max_width <= wgroup_size) {
        local_width = max_width;
        wgroup_size = wgroup_size / local_width;
        if (max_height <= wgroup_size) {
            local_height = max_height;
        } else {
            local_height = wgroup_size;
            max_height = (max_height / wgroup_size + 1) * wgroup_size;
        }
    } else {
        local_width = wgroup_size;
        local_height = 1;
        max_width = (max_width / wgroup_size + 1) * wgroup_size;
    }

    sycl::range global{texts_size, max_height, max_width};
    sycl::range local{1, local_height, local_width};
    e = queue.parallel_for<class RenderText>(sycl::nd_range{global, local}, [=](sycl::nd_item<3> item) {
        const size_t k = item.get_global_id(0);
        const size_t i = item.get_global_id(1);
        const size_t j = item.get_global_id(2);
        const auto &text = texts[k].first;

        const size_t y = (size_t)text.y + i;
        const size_t x = (size_t)text.x + j;
        const size_t offset = x + y * width;
        const size_t max_width = ((size_t)text.x + text.w > width) ? width : (size_t)text.x + text.w;
        if (x <= max_width && y <= (size_t)text.y + text.h) {
            const sycl::int2 p1(text.x, text.y);
            const sycl::int2 p2(x, y);
            const sycl::int2 p3 = p2 - p1;
            const size_t patch_offset = p3.x() + text.w * p3.y();
            if (text.bitmap[patch_offset])
                setColor(texts[k].second, mask[offset]);
        }
    });
    return e;
}

sycl::event gpu::dpcpp::renderCircles(sycl::queue queue, size_t width, gpu::dpcpp::MaskedPixel *mask,
                                      const gpu::dpcpp::Circle *circles, size_t circles_size, size_t max_radius) {
    sycl::event e;
    auto max_d = max_radius * 2;

    sycl::device device = queue.get_device();
    auto wgroup_size = device.get_info<sycl::info::device::max_work_group_size>();
    size_t local_width = 1;
    size_t local_height = 1;

    if (max_d <= wgroup_size) {
        local_width = max_d;
        wgroup_size = wgroup_size / local_width;
    } else {
        local_width = wgroup_size;
        max_d = (max_d / wgroup_size + 1) * wgroup_size;
    }

    sycl::range global{circles_size, max_d, max_d};
    sycl::range local{1, local_height, local_width};
    e = queue.parallel_for<class RenderCircle>(sycl::nd_range{global, local}, [=](sycl::nd_item<3> item) {
        const int k = item.get_global_id(0);
        const int i = item.get_global_id(1);
        const int j = item.get_global_id(2);

        const cv::gapi::wip::draw::Circle &circle = circles[k].first;
        const int r2 = circle.radius * circle.radius + 1;

        const size_t y = circle.center.y - circle.radius + i;
        const size_t x = circle.center.x - circle.radius + j;
        const int dx = circle.center.x - x;
        const int dy = circle.center.y - y;
        if (x <= width) {

            if (dx * dx + dy * dy < r2) {
                const size_t offset = x + y * width;
                setColor(circles[k].second, mask[offset]);
            }
        }
    });
    return e;
}

sycl::event gpu::dpcpp::mix(sycl::queue queue, gpu::dpcpp::MaskedPixel *mask, cv::Mat &image_plane, int plane_index,
                            gpu::dpcpp::SubsampligParams subsampling, uint64_t drm_format_modifier) {
    uint8_t *data = image_plane.data;
    int height = image_plane.rows;
    int width = image_plane.cols;
    uint32_t nchan = image_plane.channels();

    uint8_t j_step = subsampling.J / subsampling.a;
    uint8_t i_step = subsampling.b == 0 ? 2 : 1;

    int mask_width = image_plane.cols * j_step;

    sycl::event e;
    if (drm_format_modifier == I915_FORMAT_MOD_Y_TILED) {
        e = queue.parallel_for(sycl::range<2>(height, width), [=](sycl::nd_item<2> item) {
            const size_t i = item.get_global_id(0) * i_step;
            const size_t j = item.get_global_id(1) * j_step;
            if (mask[i * mask_width + j].colored) {
                int x = j / j_step * nchan;
                int y = i / i_step;
                uint8_t *pix;
                pix = data + TILED_OFFSET(x, y, mask_width);
                for (size_t subpix = 0; subpix < nchan; subpix++) {
                    pix[subpix] = mask[i * mask_width + j].ch[plane_index + subpix];
                }
            }
        });
    } else {
        e = queue.parallel_for(sycl::range<2>(height, width), [=](sycl::nd_item<2> item) {
            const size_t i = item.get_global_id(0) * i_step;
            const size_t j = item.get_global_id(1) * j_step;
            if (mask[i * mask_width + j].colored) {
                int x = j / j_step * nchan;
                int y = i / i_step;
                uint8_t *pix;
                pix = data + y * width + x;
                for (size_t subpix = 0; subpix < nchan; subpix++) {
                    pix[subpix] = mask[i * mask_width + j].ch[plane_index + subpix];
                }
            }
        });
    }
    return e;
}
