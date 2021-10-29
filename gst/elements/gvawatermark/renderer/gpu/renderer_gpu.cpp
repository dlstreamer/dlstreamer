/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer_gpu.h"

#include "dpcpp_draw.h"
#include "inference_backend/logger.h"
#include "usm_buffer_map.h"

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

gpu::dpcpp::Rect RendererGPU::prepare_rectangle(gapidraw::Rect rect, int &max_side) {
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

std::vector<gpu::dpcpp::Text> RendererGPU::prepare_text(const gapidraw::Text &drawing_text, int &max_width,
                                                        int &max_height) {
    std::vector<gpu::dpcpp::Text> tmp_texts;
    const std::string label = drawing_text.text + " ";
    std::string::size_type last_pos = 0;
    cv::Point sub_label_position = drawing_text.org;
    for (auto pos = label.find(" "); pos != std::string::npos; pos = label.find(" ", last_pos)) {
        std::string sub_label = label.substr(last_pos, pos - last_pos);
        if (not text_storage.count(sub_label)) {
            int baseline = 0;
            cv::Size text_size =
                cv::getTextSize(sub_label, drawing_text.ff, drawing_text.fs, drawing_text.thick, &baseline);
            text_size.height += baseline;
            uint8_t *text_data = sycl::malloc_device<uint8_t>(text_size.area(), *queue);
            cv::Mat m = cv::Mat::zeros(text_size.height, text_size.width, CV_8UC1);
            cv::putText(m, sub_label, {0, text_size.height - baseline}, drawing_text.ff, drawing_text.fs, 255,
                        drawing_text.thick, drawing_text.lt, false);
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
            cv::getTextSize(" ", drawing_text.ff, drawing_text.fs, drawing_text.thick, 0);
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

void RendererGPU::malloc_device_prims(int prim_type, uint32_t size) {
    switch (prim_type) {
    case gapidraw::Prim::index_of<gapidraw::Rect>(): {
        if (_rectangles_size < size) {
            _rectangles_size = size;
            rectangles = gpu_unique_ptr<gpu::dpcpp::Rect>(
                sycl::malloc_device<gpu::dpcpp::Rect>(_rectangles_size, *queue),
                [this](gpu::dpcpp::Rect *rectangles) { sycl::free(rectangles, *queue); });
        }
    }
    case gapidraw::Prim::index_of<gapidraw::Circle>(): {
        if (_circles_size < size) {
            _circles_size = size;
            circles = gpu_unique_ptr<gpu::dpcpp::Circle>(
                sycl::malloc_device<gpu::dpcpp::Circle>(_circles_size, *queue),
                [this](gpu::dpcpp::Circle *circles) { sycl::free(circles, *queue); });
        }
    }
    case gapidraw::Prim::index_of<gapidraw::Text>(): {
        if (_texts_size < size) {
            _texts_size = size;
            texts = gpu_unique_ptr<gpu::dpcpp::Text>(sycl::malloc_device<gpu::dpcpp::Text>(_texts_size, *queue),
                                                     [this](gpu::dpcpp::Text *texts) { sycl::free(texts, *queue); });
        }
    }
    case gapidraw::Prim::index_of<gapidraw::Line>(): {
        if (_lines_size < size) {
            _lines_size = size;
            lines = gpu_unique_ptr<gpu::dpcpp::Line>(sycl::malloc_device<gpu::dpcpp::Line>(_lines_size, *queue),
                                                     [this](gpu::dpcpp::Line *lines) { sycl::free(lines, *queue); });
        }
    }
    }
}

void RendererRGB::draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) {
    ITT_TASK(__FUNCTION__);

    std::vector<gpu::dpcpp::Rect> tmp_rectangles;
    std::vector<gpu::dpcpp::Circle> tmp_circles;
    std::vector<gpu::dpcpp::Line> tmp_lines;
    tmp_rectangles.reserve(prims.size());
    tmp_circles.reserve(prims.size());
    tmp_lines.reserve(prims.size());

    std::vector<gpu::dpcpp::Text> tmp_texts;
    auto rect_max_side = 0;
    auto text_max_width = 0;
    auto text_max_height = 0;
    auto max_radius = 0;

    for (const auto &p : prims) {
        switch (p.index()) {
        case gapidraw::Prim::index_of<gapidraw::Rect>(): {
            tmp_rectangles.emplace_back(prepare_rectangle(cv::util::get<gapidraw::Rect>(p), rect_max_side));
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Text>(): {
            auto texts_to_append = prepare_text(cv::util::get<gapidraw::Text>(p), text_max_width, text_max_height);
            tmp_texts.insert(tmp_texts.end(), texts_to_append.begin(), texts_to_append.end());
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Circle>(): {
            const auto &circle = cv::util::get<gapidraw::Circle>(p);
            if (circle.radius > max_radius) {
                max_radius = circle.radius;
            }
            tmp_circles.emplace_back(std::pair(circle, Color(circle.color)));
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Line>(): {
            const auto &line = cv::util::get<gapidraw::Line>(p);
            tmp_lines.emplace_back(std::pair(line, Color(line.color)));
            break;
        }
        default:
            throw std::runtime_error("Unsupported primitive type");
        }
    }
    sycl::event e0, e1, e2, e3;
    if (!tmp_rectangles.empty()) {
        uint32_t rectangles_size = tmp_rectangles.size();
        malloc_device_prims(gapidraw::Prim::index_of<gapidraw::Rect>(), rectangles_size);
        queue->memcpy(rectangles.get(), tmp_rectangles.data(), rectangles_size * sizeof(gpu::dpcpp::Rect)).wait();
        e0 = gpu::dpcpp::renderRectangles(*queue, image_planes[0], rectangles.get(), rectangles_size, rect_max_side);
    }

    if (!tmp_circles.empty()) {
        uint32_t circles_size = tmp_circles.size();
        malloc_device_prims(gapidraw::Prim::index_of<gapidraw::Circle>(), circles_size);
        queue->memcpy(circles.get(), tmp_circles.data(), tmp_circles.size() * sizeof(gpu::dpcpp::Circle)).wait();
        e1 = gpu::dpcpp::renderCircles(*queue, image_planes[0], circles.get(), circles_size, max_radius);
    }

    if (!tmp_lines.empty()) {
        uint32_t lines_size = tmp_lines.size();
        malloc_device_prims(gapidraw::Prim::index_of<gapidraw::Line>(), lines_size);
        queue->memcpy(lines.get(), tmp_lines.data(), tmp_lines.size() * sizeof(gpu::dpcpp::Line)).wait();
        e3 = gpu::dpcpp::renderLines(*queue, image_planes[0], lines.get(), lines_size, tmp_lines[0].first.thick);
    }

    if (!tmp_texts.empty()) {
        uint32_t texts_size = tmp_texts.size();
        malloc_device_prims(gapidraw::Prim::index_of<gapidraw::Text>(), texts_size);
        queue->memcpy(texts.get(), tmp_texts.data(), tmp_texts.size() * sizeof(gpu::dpcpp::Text)).wait();
        e2 = gpu::dpcpp::renderTexts(*queue, image_planes[0], texts.get(), texts_size, text_max_height, text_max_width);
    }
    e0.wait();
    e1.wait();
    e2.wait();
    e3.wait();
}

void RendererGPU::buffer_map(GstBuffer *buffer, InferenceBackend::Image &image) {
    image = buffer_mapper->map(buffer, GST_MAP_READWRITE);
}

void RendererGPU::buffer_unmap(InferenceBackend::Image &image) {
    buffer_mapper->unmap(image);
}

RendererGPU::RendererGPU(std::shared_ptr<ColorConverter> color_converter,
                         std::unique_ptr<BufferMapper> input_buffer_mapper, int image_width, int image_height)
    : Renderer(color_converter, InferenceBackend::MemoryType::USM_DEVICE_POINTER), image_width(image_width),
      image_height(image_height) {
    queue = std::make_shared<sycl::queue>(sycl::gpu_selector());
    buffer_mapper = std::make_shared<UsmBufferMapper>(queue, std::move(input_buffer_mapper));
}

RendererGPU::~RendererGPU() {
    for (auto &t : text_storage)
        if (t.second.map)
            sycl::free(t.second.map, *queue);
}
