/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer.h"

#include "cpu/renderer_cpu.h"

std::vector<cv::Mat> Renderer::convertBufferToCvMats(dlstreamer::Buffer &buffer) {
    static std::array<int, 4> channels_to_cvtype_map = {CV_8UC1, CV_8UC2, CV_8UC3, CV_8UC4};

    auto &info = *buffer.info();
    assert(info.media_type == dlstreamer::MediaType::VIDEO);
    assert(!info.planes.empty());

    std::vector<cv::Mat> image_planes;
    image_planes.reserve(info.planes.size());

    // Check for supported formats
    using dlstreamer::FourCC;
    switch (info.format) {
    case FourCC::FOURCC_BGRX:
    case FourCC::FOURCC_BGR:
    case FourCC::FOURCC_RGBX:
    case FourCC::FOURCC_RGB:
    case FourCC::FOURCC_I420:
    case FourCC::FOURCC_NV12:
        break;
    default:
        throw std::runtime_error("Unsupported image format");
    }

    // Go through planes and create cv::Mat for every plane
    size_t plane_idx = 0;
    for (auto &plane : info.planes) {
        // Verify number of channels
        assert(plane.channels() > 0 && plane.channels() <= channels_to_cvtype_map.size());
        const int cv_type = channels_to_cvtype_map[plane.channels() - 1];

        image_planes.emplace_back(plane.height(), plane.width(), cv_type, buffer.data(plane_idx++),
                                  plane.width_stride());
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

void Renderer::draw(dlstreamer::BufferPtr buffer, std::vector<gapidraw::Prim> prims) {
    auto mapped_buf = buffer_map(buffer);
    std::vector<cv::Mat> image_planes = convertBufferToCvMats(*mapped_buf);
    convert_prims_color(prims);
    draw_backend(image_planes, prims);
}
