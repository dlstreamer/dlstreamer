/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer_cpu.h"

#include "render_prim.h"
#include <inference_backend/buffer_mapper.h>

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

dlstreamer::FramePtr RendererCPU::buffer_map(dlstreamer::FramePtr buffer) {
    auto result = _buffer_mapper->map(buffer, dlstreamer::AccessMode::ReadWrite);
    return result;
}

void RendererYUV::draw_backend(std::vector<cv::Mat> &image_planes, std::vector<render::Prim> &prims) {
    for (auto &p : prims) {
        if (std::holds_alternative<render::Line>(p)) {
            draw_line(image_planes, std::get<render::Line>(p));
        } else if (std::holds_alternative<render::Rect>(p)) {
            draw_rectangle(image_planes, std::get<render::Rect>(p));
        } else if (std::holds_alternative<render::Circle>(p)) {
            draw_circle(image_planes, std::get<render::Circle>(p));
        } else if (std::holds_alternative<render::Text>(p)) {
            draw_text(image_planes, std::get<render::Text>(p));
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

void RendererI420::draw_rectangle(std::vector<cv::Mat> &mats, render::Rect rect) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    const cv::Point2i top_left = rect.rect.tl();
    // align with render::render behavior
    const cv::Point2i bottom_right = rect.rect.br() - cv::Point2i(1, 1);

    const int thick = calc_thick_for_u_v_planes(rect.thick);
    cv::rectangle(u, calc_point_for_u_v_planes(top_left), calc_point_for_u_v_planes(bottom_right), rect.color[1],
                  thick);
    cv::rectangle(v, calc_point_for_u_v_planes(top_left), calc_point_for_u_v_planes(bottom_right), rect.color[2],
                  thick);

    draw_rect_y_plane(y, top_left, bottom_right, rect.color[0], rect.thick);
}

void RendererI420::draw_circle(std::vector<cv::Mat> &mats, render::Circle circle) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    cv::circle(y, circle.center, circle.radius, circle.color[0], cv::FILLED);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(circle.center));
    cv::circle(u, pos_u_v, circle.radius / 2, circle.color[1], cv::FILLED);
    cv::circle(v, pos_u_v, circle.radius / 2, circle.color[2], cv::FILLED);
}

void RendererI420::draw_text(std::vector<cv::Mat> &mats, render::Text text) {
    check_planes<3>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u = mats[1];
    cv::Mat &v = mats[2];

    cv::putText(y, text.text, text.org, text.fonttype, text.fontscale, text.color[0], text.thick);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(text.org));
    int thick = calc_thick_for_u_v_planes(text.thick);
    cv::putText(u, text.text, pos_u_v, text.fonttype, text.fontscale / 2.0, text.color[1], thick);
    cv::putText(v, text.text, pos_u_v, text.fonttype, text.fontscale / 2.0, text.color[2], thick);
}

void RendererI420::draw_line(std::vector<cv::Mat> &mats, render::Line line) {
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

void RendererNV12::draw_rectangle(std::vector<cv::Mat> &mats, render::Rect rect) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    const cv::Point2i top_left = rect.rect.tl();
    // align with render::render behavior
    const cv::Point2i bottom_right = rect.rect.br() - cv::Point2i(1, 1);

    cv::rectangle(u_v, calc_point_for_u_v_planes(top_left), calc_point_for_u_v_planes(bottom_right),
                  {rect.color[1], rect.color[2]}, calc_thick_for_u_v_planes(rect.thick));

    draw_rect_y_plane(y, top_left, bottom_right, rect.color[0], rect.thick);
}

void RendererNV12::draw_circle(std::vector<cv::Mat> &mats, render::Circle circle) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    cv::circle(y, circle.center, circle.radius, circle.color[0], cv::FILLED);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(circle.center));
    cv::circle(u_v, pos_u_v, circle.radius / 2, {circle.color[1], circle.color[2]}, cv::FILLED);
}

void RendererNV12::draw_text(std::vector<cv::Mat> &mats, render::Text text) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    cv::putText(y, text.text, text.org, text.fonttype, text.fontscale, text.color[0], text.thick);
    cv::Point2i pos_u_v(calc_point_for_u_v_planes(text.org));
    cv::putText(u_v, text.text, pos_u_v, text.fonttype, text.fontscale / 2.0, {text.color[1], text.color[2]},
                calc_thick_for_u_v_planes(text.thick));
}

void RendererNV12::draw_line(std::vector<cv::Mat> &mats, render::Line line) {
    check_planes<2>(mats);
    cv::Mat &y = mats[0];
    cv::Mat &u_v = mats[1];

    cv::line(y, line.pt1, line.pt2, line.color[0], line.thick);
    cv::Point2f pos1_u_v(calc_point_for_u_v_planes(line.pt1));
    cv::Point2f pos2_u_v(calc_point_for_u_v_planes(line.pt2));
    cv::line(u_v, pos1_u_v, pos2_u_v, {line.color[1], line.color[2]}, calc_thick_for_u_v_planes(line.thick));
}

void RendererBGR::draw_rectangle(std::vector<cv::Mat> &mats, render::Rect rect) {
    cv::rectangle(mats[0], rect.rect.tl(), rect.rect.br(), rect.color, rect.thick);
}

void RendererBGR::draw_circle(std::vector<cv::Mat> &mats, render::Circle circle) {
    cv::circle(mats[0], circle.center, circle.radius, circle.color, cv::FILLED);
}

void RendererBGR::draw_text(std::vector<cv::Mat> &mats, render::Text text) {
    cv::putText(mats[0], text.text, text.org, text.fonttype, text.fontscale, text.color, text.thick);
}

void RendererBGR::draw_line(std::vector<cv::Mat> &mats, render::Line line) {
    cv::line(mats[0], line.pt1, line.pt2, line.color, line.thick);
}
