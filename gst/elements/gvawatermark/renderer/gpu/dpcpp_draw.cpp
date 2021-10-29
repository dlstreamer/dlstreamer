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

sycl::event gpu::dpcpp::renderLines(sycl::queue queue, cv::Mat &image_plane, const gpu::dpcpp::Line *lines,
                                    size_t lines_size, size_t thick) {
    sycl::event e;
    sycl::range global{lines_size, thick};
    sycl::range local{1, 1};
    uint8_t *data = image_plane.data;
    size_t width = image_plane.cols;
    const auto nchan = image_plane.channels();
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
            const size_t offset = (x_c + y_c * width) * nchan;
            setColor(lines[k].second, &data[offset]);
            error -= dy;
            if (error < 0) {
                y += ystep;
                error += dx;
            }
        }
    });
    return e;
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

        const cv::gapi::wip::draw::Circle &circle = circles[k].first;
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
