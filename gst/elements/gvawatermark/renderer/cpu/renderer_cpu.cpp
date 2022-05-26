/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer_cpu.h"

#include <inference_backend/buffer_mapper.h>
#include <opencv2/gapi/render/render.hpp>

namespace {
template <int n>
void check_planes(const std::vector<cv::Mat> &p) {
    if (p.size() != n) {
        throw std::runtime_error("plane count != " + std::to_string(n));
    }
}

int calc_thick_for_u_v_planes(int thick) {
    if (thick <= 1)
        return thick;
    return thick / 2;
}

// cv::Point2f calc_point_for_u_v_planes(cv::Point2f pt) {
//     return pt / 2.f;
// }

cv::Point2i calc_point_for_u_v_planes(cv::Point2i pt) {
    return pt / 2;
}

} // namespace

RendererCPU::~RendererCPU() {
}

dlstreamer::BufferPtr RendererCPU::buffer_map(dlstreamer::BufferPtr buffer) {
    auto result = _buffer_mapper->map(buffer, dlstreamer::AccessMode::READ_WRITE);
    return result;
}

void RendererYUV::draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) {
    for (auto &p : prims) {
        switch (p.index()) {
        case gapidraw::Prim::index_of<gapidraw::Line>(): {
            draw_line(image_planes, cv::util::get<gapidraw::Line>(p));
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Rect>(): {
            draw_rectangle(image_planes, cv::util::get<gapidraw::Rect>(p));
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Circle>(): {
            draw_circle(image_planes, cv::util::get<gapidraw::Circle>(p));
            break;
        }
        case gapidraw::Prim::index_of<gapidraw::Text>(): {
            draw_text(image_planes, cv::util::get<gapidraw::Text>(p));
            break;
        }
        }
    }
}

void RendererYUV::draw_rect_y_plane(cv::Mat &y, cv::Point2i pt1, cv::Point2i pt2, double color, int thick) {
    // Half thickness
    thick = calc_thick_for_u_v_planes(thick);

    // Pixel perfect rectangle.
    // Every pixel on U & V planes corresponds to two pixels on Y plane.
    // So, to avoid shadows, there's need to fill two pixels on Y plane, for every single pixel on U & V planes.
    // Since coordinates may not be a multiple of two, the two rectangles should be drawn with half thickness on Y
    // plane.
    cv::rectangle(y, pt1, pt2, color, thick);
    float offset_min_x = pt1.x % 2 ? -1 : 1;
    float offset_min_y = pt1.y % 2 ? -1 : 1;
    float offset_max_x = pt2.x % 2 ? -1 : 1;
    float offset_max_y = pt2.y % 2 ? -1 : 1;
    cv::Point2i p1(offset_min_x + pt1.x, offset_min_y + pt1.y);
    cv::Point2i p2(offset_max_x + pt2.x, offset_max_y + pt2.y);
    cv::rectangle(y, p1, p2, color, thick);
}

void RendererI420::draw_rectangle(std::vector<cv::Mat> &mats, gapidraw::Rect rect) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    const cv::Point2i top_left = rect.rect.tl();
    // align with gapidraw::render behavior
    const cv::Point2i bottom_right = rect.rect.br() - cv::Point2i(1, 1);

    const int thick = calc_thick_for_u_v_planes(rect.thick);
    cv::rectangle(u, calc_point_for_u_v_planes(top_left), calc_point_for_u_v_planes(bottom_right), rect.color[1],
                  thick);
    cv::rectangle(v, calc_point_for_u_v_planes(top_left), calc_point_for_u_v_planes(bottom_right), rect.color[2],
                  thick);

    draw_rect_y_plane(y, top_left, bottom_right, rect.color[0], rect.thick);
}

void RendererI420::draw_circle(std::vector<cv::Mat> &mats, gapidraw::Circle circle) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    cv::circle(y, circle.center, circle.radius, circle.color[0], cv::FILLED);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(circle.center));
    cv::circle(u, pos_u_v, circle.radius / 2, circle.color[1], cv::FILLED);
    cv::circle(v, pos_u_v, circle.radius / 2, circle.color[2], cv::FILLED);
}

void RendererI420::draw_text(std::vector<cv::Mat> &mats, gapidraw::Text text) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    cv::putText(y, text.text, text.org, text.ff, text.fs, text.color[0], text.thick);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(text.org));
    int thick = calc_thick_for_u_v_planes(text.thick);
    cv::putText(u, text.text, pos_u_v, text.ff, text.fs / 2.0, text.color[1], thick);
    cv::putText(v, text.text, pos_u_v, text.ff, text.fs / 2.0, text.color[2], thick);
}

void RendererI420::draw_line(std::vector<cv::Mat> &mats, gapidraw::Line line) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    cv::line(y, line.pt1, line.pt2, line.color[0], line.thick);

    cv::Point2f pos1_u_v(calc_point_for_u_v_planes(line.pt1));
    cv::Point2f pos2_u_v(calc_point_for_u_v_planes(line.pt2));
    int thick = calc_thick_for_u_v_planes(line.thick);
    cv::line(u, pos1_u_v, pos2_u_v, line.color[1], thick);
    cv::line(v, pos1_u_v, pos2_u_v, line.color[2], thick);
}

void RendererNV12::draw_rectangle(std::vector<cv::Mat> &mats, gapidraw::Rect rect) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    const cv::Point2i top_left = rect.rect.tl();
    // align with gapidraw::render behavior
    const cv::Point2i bottom_right = rect.rect.br() - cv::Point2i(1, 1);

    cv::rectangle(u_v, calc_point_for_u_v_planes(top_left), calc_point_for_u_v_planes(bottom_right),
                  {rect.color[1], rect.color[2]}, calc_thick_for_u_v_planes(rect.thick));

    draw_rect_y_plane(y, top_left, bottom_right, rect.color[0], rect.thick);
}

void RendererNV12::draw_circle(std::vector<cv::Mat> &mats, gapidraw::Circle circle) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    cv::circle(y, circle.center, circle.radius, circle.color[0], cv::FILLED);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(circle.center));
    cv::circle(u_v, pos_u_v, circle.radius / 2, {circle.color[1], circle.color[2]}, cv::FILLED);
}

void RendererNV12::draw_text(std::vector<cv::Mat> &mats, gapidraw::Text text) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    cv::putText(y, text.text, text.org, text.ff, text.fs, text.color[0], text.thick);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(text.org));
    cv::putText(u_v, text.text, pos_u_v, text.ff, text.fs / 2.0, {text.color[1], text.color[2]},
                calc_thick_for_u_v_planes(text.thick));
}

void RendererNV12::draw_line(std::vector<cv::Mat> &mats, gapidraw::Line line) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    cv::line(y, line.pt1, line.pt2, line.color[0], line.thick);
    cv::Point2f pos1_u_v(calc_point_for_u_v_planes(line.pt1));
    cv::Point2f pos2_u_v(calc_point_for_u_v_planes(line.pt2));
    cv::line(u_v, pos1_u_v, pos2_u_v, {line.color[1], line.color[2]}, calc_thick_for_u_v_planes(line.thick));
}

void RendererBGR::draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) {
    gapidraw::render(image_planes[0], prims);
}
