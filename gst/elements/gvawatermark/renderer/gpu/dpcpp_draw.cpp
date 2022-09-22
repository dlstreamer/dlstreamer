/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
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

namespace {

inline void setColor(const Color &src, uint8_t *dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
};
} // namespace

sycl::event gpu::dpcpp::renderRectangles(sycl::queue queue, cv::Mat &image_plane, const gpu::dpcpp::Rect *rectangles,
                                         size_t rectangles_size, size_t max_length) {
    uint8_t *data = image_plane.data;
    size_t width = image_plane.cols;
    const auto nchan = image_plane.channels();

    sycl::event e;
    size_t local_length = 0;
    sycl::device device = queue.get_device();
    size_t wgroup_size = device.get_info<sycl::info::device::max_work_group_size>();

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
                setColor(draw_rect.second, &data[(x + (rect.y + j) * width) * nchan]);
                setColor(draw_rect.second, &data[(x + (rect.y + rect.height + thick + j) * width) * nchan]);
            }
        }
        if (y <= rect.y + rect.height + thick) {
            for (auto j = 0; j < thick; j++) {
                setColor(draw_rect.second, &data[((rect.x + j) + y * width) * nchan]);
                setColor(draw_rect.second, &data[((rect.x + rect.width + thick + j) + y * width) * nchan]);
            }
        }
    });
    return e;
}

sycl::event gpu::dpcpp::renderLinesLow(sycl::queue queue, cv::Mat &image_plane, const gpu::dpcpp::Line *lines,
                                       size_t lines_size, size_t thick) {

    sycl::range global{lines_size, thick};
    sycl::range local{1, 1};
    uint8_t *data = image_plane.data;
    const size_t width = image_plane.cols;
    const auto num_ch = image_plane.channels();

    return queue.parallel_for<class RenderLineLow>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
        const int k = item.get_global_id(0); // Index of line in array
        const int i = item.get_global_id(1);

        const auto &line = lines[k];
        // Thikness handling
        const auto y0 = line.y0 + i;
        const auto y1 = line.y1 + i;

        const int dx = abs(line.x1 - line.x0);
        const int dy = abs(y1 - y0);

        // y increment lookup table
        const int look_y[] = {0, (y0 < y1) ? 1 : -1};
        // Error lookup table
        const int look_err[] = {dy, dy - dx};
        // Initial error value
        int error = dy - dx / 2;

        int y = y0;
        for (int x = line.x0; x != line.x1 + 1; x++) {
            size_t offset = (x + y * width) * num_ch;
            setColor(line.color, &data[offset]);

            const bool ec = error >= 0;
            // Increment y and error values based on error check value
            y += look_y[ec];
            error += look_err[ec];
        }
    });
}

sycl::event gpu::dpcpp::renderLinesHi(sycl::queue queue, cv::Mat &image_plane, const gpu::dpcpp::Line *lines,
                                      size_t lines_size, size_t thick) {

    sycl::range global{lines_size, thick};
    sycl::range local{1, 1};
    uint8_t *data = image_plane.data;
    const size_t width = image_plane.cols;
    const auto num_ch = image_plane.channels();

    return queue.parallel_for<class RenderLineHi>(sycl::nd_range{global, local}, [=](sycl::nd_item<2> item) {
        const int k = item.get_global_id(0); // Index of line in array
        const int i = item.get_global_id(1);

        const auto &line = lines[k];
        // Thikness handling
        const auto x0 = line.x0 + i;
        const auto x1 = line.x1 + i;

        const int dx = abs(x1 - x0);
        const int dy = abs(line.y1 - line.y0);

        // x increment lookup table
        const int look_x[] = {0, (x0 < x1) ? 1 : -1};
        // Error lookup table
        const int look_err[] = {dx, dx - dy};
        // Initial error value
        int error = dx - dy / 2;

        int x = x0;
        for (int y = line.y0; y != line.y1 + 1; y++) {
            size_t offset = (x + y * width) * num_ch;
            setColor(line.color, &data[offset]);

            const bool ec = error >= 0;
            // Increment y and error values based on error check value
            x += look_x[ec];
            error += look_err[ec];
        }
    });
}

sycl::event gpu::dpcpp::renderTexts(sycl::queue queue, cv::Mat &image_plane, const gpu::dpcpp::Text *texts,
                                    size_t texts_size, size_t max_height, size_t max_width) {
    sycl::event e;
    sycl::device device = queue.get_device();
    size_t wgroup_size = device.get_info<sycl::info::device::max_work_group_size>();
    uint8_t *data = image_plane.data;
    size_t width = image_plane.cols;
    const auto nchan = image_plane.channels();
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

        const size_t y = static_cast<size_t>(text.y) + i;
        const size_t x = static_cast<size_t>(text.x) + j;
        const size_t offset = (x + y * width) * nchan;
        const size_t max_width =
            (static_cast<size_t>(text.x) + text.w > width) ? width : static_cast<size_t>(text.x) + text.w;
        if (x <= max_width && y <= static_cast<size_t>(text.y) + text.h) {
            const sycl::int2 p1(text.x, text.y);
            const sycl::int2 p2(x, y);
            const sycl::int2 p3 = p2 - p1;
            const size_t patch_offset = p3.x() + text.w * p3.y();
            if (text.bitmap[patch_offset])
                setColor(texts[k].second, &data[offset]);
        }
    });
    return e;
}

sycl::event gpu::dpcpp::renderCircles(sycl::queue queue, cv::Mat &image_plane, const gpu::dpcpp::Circle *circles,
                                      size_t circles_size, size_t max_radius) {
    sycl::event e;
    auto max_d = max_radius * 2;
    uint8_t *data = image_plane.data;
    size_t width = image_plane.cols;
    const auto nchan = image_plane.channels();
    sycl::device device = queue.get_device();
    size_t wgroup_size = device.get_info<sycl::info::device::max_work_group_size>();
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

        const render::Circle &circle = circles[k].first;
        const int r2 = circle.radius * circle.radius + 1;

        const size_t y = circle.center.y - circle.radius + i;
        const size_t x = circle.center.x - circle.radius + j;
        const int dx = circle.center.x - x;
        const int dy = circle.center.y - y;
        if (x <= width) {

            if (dx * dx + dy * dy < r2) {
                const size_t offset = (x + y * width) * nchan;
                setColor(circles[k].second, &data[offset]);
            }
        }
    });
    return e;
}
