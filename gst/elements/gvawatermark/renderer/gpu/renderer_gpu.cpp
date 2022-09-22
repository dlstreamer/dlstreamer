/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer_gpu.h"

#include "buffer_mapper/dma_to_usm.h"
#include "dpcpp_draw.h"
#include "inference_backend/logger.h"
#include <dlstreamer/level_zero/context.h>

#include <level_zero/ze_api.h>

#include <CL/sycl.hpp>
#include <CL/sycl/backend.hpp>
#include <CL/sycl/backend/level_zero.hpp>
#include <CL/sycl/usm.hpp>

#include <drm/drm_fourcc.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

using namespace gpu::draw;

gpu::dpcpp::Rect RendererGPU::prepare_rectangle(render::Rect rect, int &max_side) {
    rect.rect.x = rect.rect.x & ~1;
    rect.rect.y = rect.rect.y & ~1;
    rect.rect.height = rect.rect.height & ~1;
    rect.rect.width = rect.rect.width & ~1;
    if (rect.thick == 1) {
        rect.thick = 2;
    }
    rect.thick = rect.thick & ~1;

    if (rect.rect.height + 2 * rect.thick > max_side) {
        max_side = rect.rect.height + 2 * rect.thick;
    }
    if (rect.rect.width + 2 * rect.thick > max_side) {
        max_side = rect.rect.width + 2 * rect.thick;
    }

    if (rect.rect.x < 0)
        rect.rect.x = 0;
    if (rect.rect.y < 0)
        rect.rect.y = 0;
    if (rect.rect.x + rect.rect.width + 2 * rect.thick > image_width)
        rect.rect.width = image_width - rect.rect.x - 2 * rect.thick;
    if (rect.rect.y + rect.rect.height + 2 * rect.thick > image_height)
        rect.rect.height = image_height - rect.rect.y - 2 * rect.thick;
    return std::pair(rect, Color(rect.color));
}

std::vector<gpu::dpcpp::Text> RendererGPU::prepare_text(const render::Text &drawing_text, int &max_width,
                                                        int &max_height) {
    std::vector<gpu::dpcpp::Text> tmp_texts;
    const std::string label = drawing_text.text + " ";
    std::string::size_type last_pos = 0;
    cv::Point sub_label_position = drawing_text.org;
    for (auto pos = label.find(" "); pos != std::string::npos; pos = label.find(" ", last_pos)) {
        std::string sub_label = label.substr(last_pos, pos - last_pos);
        if (not text_storage.count(sub_label)) {
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(sub_label, drawing_text.fonttype, drawing_text.fontscale,
                                                 drawing_text.thick, &baseline);
            text_size.height += baseline;
            uint8_t *text_data = sycl::malloc_device<uint8_t>(text_size.area(), *queue);
            cv::Mat m = cv::Mat::zeros(text_size.height, text_size.width, CV_8UC1);
            cv::putText(m, sub_label, {0, text_size.height - baseline}, drawing_text.fonttype, drawing_text.fontscale,
                        255, drawing_text.thick);
            queue->memcpy(text_data, m.data, text_size.area() * sizeof(uint8_t)).wait();
            text_storage[sub_label] = {text_data, text_size, baseline};
        }
        gpu::dpcpp::RasterText t = {
            .bitmap = text_storage[sub_label].map,
            .x = sub_label_position.x,
            .y = sub_label_position.y - text_storage[sub_label].size.height + text_storage[sub_label].baseline,
            .w = text_storage[sub_label].size.width,
            .h = text_storage[sub_label].size.height,
        };
        tmp_texts.push_back(gpu::dpcpp::Text(t, Color(drawing_text.color)));
        static const cv::Size space_size =
            cv::getTextSize(" ", drawing_text.fonttype, drawing_text.fontscale, drawing_text.thick, 0);
        sub_label_position.x += text_storage[sub_label].size.width + space_size.width;
        last_pos = pos + 1;
        if (t.h > max_height) {
            max_height = t.h;
        }
        if (t.w > max_width) {
            max_width = t.w;
        }
    }
    return tmp_texts;
}

gpu::dpcpp::Line RendererGPU::prepare_line(const render::Line &line) {
    gpu::dpcpp::Line l;
    l.x0 = line.pt1.x;
    l.y0 = line.pt1.y;
    l.x1 = line.pt2.x;
    l.y1 = line.pt2.y;
    l.color = Color(line.color);

    const int dx = l.x1 - l.x0;
    const int dy = l.y1 - l.y0;
    l.steep = abs(dy) > abs(dx);
    bool swap = false;
    if (l.steep)
        swap = dy < 0;
    else
        swap = dx < 0;

    if (swap) {
        std::swap(l.x0, l.x1);
        std::swap(l.y0, l.y1);
    }

    return l;
}

void RendererGPU::malloc_device_prims(render::Prim p, uint32_t size) {
    if (std::holds_alternative<render::Rect>(p)) {
        if (_rectangles_size < size) {
            _rectangles_size = size;
            rectangles = gpu_unique_ptr<gpu::dpcpp::Rect>(
                sycl::malloc_device<gpu::dpcpp::Rect>(_rectangles_size, *queue),
                [this](gpu::dpcpp::Rect *rectangles) { sycl::free(rectangles, *queue); });
        }
    } else if (std::holds_alternative<render::Circle>(p)) {
        if (_circles_size < size) {
            _circles_size = size;
            circles = gpu_unique_ptr<gpu::dpcpp::Circle>(
                sycl::malloc_device<gpu::dpcpp::Circle>(_circles_size, *queue),
                [this](gpu::dpcpp::Circle *circles) { sycl::free(circles, *queue); });
        }
    } else if (std::holds_alternative<render::Text>(p)) {
        if (_texts_size < size) {
            _texts_size = size;
            texts = gpu_unique_ptr<gpu::dpcpp::Text>(sycl::malloc_device<gpu::dpcpp::Text>(_texts_size, *queue),
                                                     [this](gpu::dpcpp::Text *texts) { sycl::free(texts, *queue); });
        }
    } else if (std::holds_alternative<render::Line>(p)) {
        if (_lines_size < size) {
            _lines_size = size;
            lines = gpu_unique_ptr<gpu::dpcpp::Line>(sycl::malloc_device<gpu::dpcpp::Line>(_lines_size, *queue),
                                                     [this](gpu::dpcpp::Line *lines) { sycl::free(lines, *queue); });
        }
    }
}

void RendererRGB::draw_backend(std::vector<cv::Mat> &image_planes, std::vector<render::Prim> &prims) {
    ITT_TASK(__FUNCTION__);

    std::vector<gpu::dpcpp::Rect> tmp_rectangles;
    std::vector<gpu::dpcpp::Circle> tmp_circles;
    std::vector<gpu::dpcpp::Line> tmp_lines_hi;
    std::vector<gpu::dpcpp::Line> tmp_lines_low;
    tmp_rectangles.reserve(prims.size());
    tmp_circles.reserve(prims.size());
    tmp_lines_hi.reserve(prims.size());
    tmp_lines_low.reserve(prims.size());

    std::vector<gpu::dpcpp::Text> tmp_texts;
    auto rect_max_side = 0;
    auto text_max_width = 0;
    auto text_max_height = 0;
    auto max_radius = 0;
    auto lines_think = 0; // Thikness of lines

    for (const auto &p : prims) {
        if (std::holds_alternative<render::Rect>(p)) {
            tmp_rectangles.emplace_back(prepare_rectangle(std::get<render::Rect>(p), rect_max_side));
        } else if (std::holds_alternative<render::Text>(p)) {
            auto texts_to_append = prepare_text(std::get<render::Text>(p), text_max_width, text_max_height);
            tmp_texts.insert(tmp_texts.end(), texts_to_append.begin(), texts_to_append.end());
        } else if (std::holds_alternative<render::Circle>(p)) {
            const auto &circle = std::get<render::Circle>(p);
            if (circle.radius > max_radius) {
                max_radius = circle.radius;
            }
            tmp_circles.emplace_back(std::pair(circle, Color(circle.color)));
        } else if (std::holds_alternative<render::Line>(p)) {
            auto line = prepare_line(std::get<render::Line>(p));
            if (line.steep)
                tmp_lines_hi.emplace_back(line);
            else
                tmp_lines_low.emplace_back(line);
            if (!lines_think)
                lines_think = std::get<render::Line>(p).thick;
            break;
        } else {
            throw std::runtime_error("Unsupported primitive type");
        }
    }
    sycl::event e0, e1, e2, e3, e4;
    if (!tmp_rectangles.empty()) {
        uint32_t rectangles_size = tmp_rectangles.size();
        malloc_device_prims(render::Rect(), rectangles_size);
        queue->memcpy(rectangles.get(), tmp_rectangles.data(), rectangles_size * sizeof(gpu::dpcpp::Rect)).wait();
        e0 = gpu::dpcpp::renderRectangles(*queue, image_planes[0], rectangles.get(), rectangles_size, rect_max_side);
    }

    if (!tmp_circles.empty()) {
        uint32_t circles_size = tmp_circles.size();
        malloc_device_prims(render::Circle(), circles_size);
        queue->memcpy(circles.get(), tmp_circles.data(), tmp_circles.size() * sizeof(gpu::dpcpp::Circle)).wait();
        e1 = gpu::dpcpp::renderCircles(*queue, image_planes[0], circles.get(), circles_size, max_radius);
    }

    if (!tmp_lines_low.empty() || !tmp_lines_hi.empty()) {
        const uint32_t lines_size = tmp_lines_low.size() + tmp_lines_hi.size();
        malloc_device_prims(render::Line(), lines_size);

        auto hi_offset = tmp_lines_low.size() * sizeof(gpu::dpcpp::Line);
        // "Low" lines are placed first in the array and "high" lines follows.
        e3 = queue->memcpy(lines.get(), tmp_lines_low.data(), tmp_lines_low.size() * sizeof(gpu::dpcpp::Line));
        e4 =
            queue->memcpy(lines.get() + hi_offset, tmp_lines_hi.data(), tmp_lines_hi.size() * sizeof(gpu::dpcpp::Line));
        sycl::event::wait({e3, e4});

        e3 = gpu::dpcpp::renderLinesLow(*queue, image_planes[0], lines.get(), tmp_lines_low.size(), lines_think);
        e4 = gpu::dpcpp::renderLinesHi(*queue, image_planes[0], lines.get() + hi_offset, tmp_lines_hi.size(),
                                       lines_think);
    }

    if (!tmp_texts.empty()) {
        uint32_t texts_size = tmp_texts.size();
        malloc_device_prims(render::Text(), texts_size);
        queue->memcpy(texts.get(), tmp_texts.data(), tmp_texts.size() * sizeof(gpu::dpcpp::Text)).wait();
        e2 = gpu::dpcpp::renderTexts(*queue, image_planes[0], texts.get(), texts_size, text_max_height, text_max_width);
    }

    sycl::event::wait({e0, e1, e2, e3, e4});
}

dlstreamer::FramePtr RendererGPU::buffer_map(dlstreamer::FramePtr buffer) {
    return buffer_mapper->map(buffer, dlstreamer::AccessMode::ReadWrite);
}

RendererGPU::RendererGPU(std::shared_ptr<ColorConverter> color_converter,
                         dlstreamer::MemoryMapperPtr input_buffer_mapper, int image_width, int image_height)
    : Renderer(color_converter), image_width(image_width), image_height(image_height) {
    queue = std::make_shared<sycl::queue>(sycl::gpu_selector());
    auto ze_context = queue->get_context().get_native<sycl::backend::level_zero>();
    auto ze_device = queue->get_device().get_native<sycl::backend::level_zero>();
    buffer_mapper = std::make_shared<dlstreamer::MapperDMAToUSM>(
        std::move(input_buffer_mapper), std::make_shared<dlstreamer::LevelZeroContext>(ze_context, ze_device));
}

RendererGPU::~RendererGPU() {
    for (auto &t : text_storage)
        if (t.second.map)
            sycl::free(t.second.map, *queue);
}
