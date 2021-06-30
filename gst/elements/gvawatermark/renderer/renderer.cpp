/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer.h"

#include "cpu/renderer_cpu.h"

#include "scope_guard.h"

int Renderer::FourccToOpenCVMatType(int fourcc) {
    switch (fourcc) {
    case InferenceBackend::FOURCC_BGRA:
        return CV_8UC4;
    case InferenceBackend::FOURCC_BGRX:
        return CV_8UC4;
    case InferenceBackend::FOURCC_BGR:
        return CV_8UC3;
    case InferenceBackend::FOURCC_RGBA:
        return CV_8UC4;
    case InferenceBackend::FOURCC_RGBX:
        return CV_8UC4;
    }
    throw std::invalid_argument("Could not convert fourcc to cv mat type: unsupported fourcc format");
}

std::vector<cv::Mat> Renderer::convertImageToMat(const InferenceBackend::Image &image) {
    std::vector<cv::Mat> image_planes;
    switch (image.format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        image_planes.emplace_back(image.height, image.width, FourccToOpenCVMatType(image.format), image.planes[0],
                                  image.stride[0]);
        break;
    case InferenceBackend::FOURCC_I420:
        image_planes.emplace_back(image.height, image.width, CV_8UC1, image.planes[0], image.stride[0]);
        image_planes.emplace_back(image.height / 2, image.width / 2, CV_8UC1, image.planes[1], image.stride[1]);
        image_planes.emplace_back(image.height / 2, image.width / 2, CV_8UC1, image.planes[2], image.stride[2]);
        break;
    case InferenceBackend::FOURCC_NV12:
        image_planes.emplace_back(image.height, image.width, CV_8UC1, image.planes[0], image.stride[0]);
        image_planes.emplace_back(image.height / 2, image.width / 2, CV_8UC2, image.planes[1], image.stride[1]);
        break;
    default:
        throw std::runtime_error("Unsupported image format");
    }
    return image_planes;
}

void Renderer::convert_prims_color(std::vector<gapidraw::Prim> &prims) {
    for (auto &p : prims) {
        switch (p.index()) {
        // TODO: use references
        case gapidraw::Prim::index_of<gapidraw::Line>(): {
            gapidraw::Line line = cv::util::get<gapidraw::Line>(p);
            line.color = _color_converter->convert(line.color);
            p = line;
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Rect>(): {
            gapidraw::Rect rect = cv::util::get<gapidraw::Rect>(p);
            rect.color = _color_converter->convert(rect.color);
            p = rect;
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Text>(): {
            gapidraw::Text text = cv::util::get<gapidraw::Text>(p);
            text.color = _color_converter->convert(text.color);
            p = text;
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Circle>(): {
            gapidraw::Circle circle = cv::util::get<gapidraw::Circle>(p);
            circle.color = _color_converter->convert(circle.color);
            p = circle;
            break;
        }
        }
    }
}

void Renderer::draw(GstBuffer *buffer, GstVideoInfo *info, std::vector<gapidraw::Prim> prims) {
    BufferMapContext mapContext;
    InferenceBackend::Image image;
    buffer_map(buffer, image, mapContext, info);
    auto mapContextGuard = makeScopeGuard([&] { buffer_unmap(mapContext); });
    std::vector<cv::Mat> image_planes = convertImageToMat(image);
    convert_prims_color(prims);
    draw_backend(image_planes, prims, image.drm_format_modifier);
}
