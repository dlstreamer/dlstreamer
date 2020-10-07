/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "renderer.h"

class RendererI420 : public RendererYUV {
  public:
    RendererI420(const std::vector<cv::Scalar> &rgb_colors, double Kb, double Kr) {
        convert_colors_rgb_to_yuv(Kr, Kb, rgb_colors, rgb_to_yuv_color_table, coefficient_matrix);
    }

    void draw_rectangle(std::vector<std::shared_ptr<cv::Mat>> mats, Color c, cv::Point2i bbox_min,
                        cv::Point2i bbox_max) override {
        check_planes<3>(mats);
        cv::Mat &y = *mats[0];
        cv::Mat &u = *mats[1];
        cv::Mat &v = *mats[2];
        Color yuv_color = rgb_to_yuv_color_table[c];

        cv::rectangle(u, bbox_min / 2, bbox_max / 2, yuv_color[1], 1);
        cv::rectangle(v, bbox_min / 2, bbox_max / 2, yuv_color[2], 1);

        cv::rectangle(y, bbox_min, bbox_max, yuv_color[0], 1);
        float offset_min_x = bbox_min.x % 2 ? -1 : 1;
        float offset_min_y = bbox_min.y % 2 ? -1 : 1;
        float offset_max_x = bbox_max.x % 2 ? -1 : 1;
        float offset_max_y = bbox_max.y % 2 ? -1 : 1;
        cv::Point2i p1(offset_min_x + bbox_min.x, offset_min_y + bbox_min.y);
        cv::Point2i p2(offset_max_x + bbox_max.x, offset_max_y + bbox_max.y);
        cv::rectangle(y, p1, p2, yuv_color[0], 1);
    }

    void draw_circle(std::vector<std::shared_ptr<cv::Mat>> mats, Color rgb_color, cv::Point2i pos,
                     size_t thickness) override {
        check_planes<3>(mats);
        cv::Mat &y = *mats[0];
        cv::Mat &u = *mats[1];
        cv::Mat &v = *mats[2];

        Color yuv_color = rgb_to_yuv_color_table[rgb_color];
        cv::circle(y, pos, thickness, yuv_color[0], -1);
        cv::Point2i pos_u_v(pos.x / 2, pos.y / 2);
        cv::circle(u, pos_u_v, thickness / 2, yuv_color[1], -1);
        cv::circle(v, pos_u_v, thickness / 2, yuv_color[2], -1);
    }

    void draw_text(std::vector<std::shared_ptr<cv::Mat>> mats, Color rgb_color, cv::Point2i pos,
                   const std::string &text) override {
        check_planes<3>(mats);
        cv::Mat &y = *mats[0];
        cv::Mat &u = *mats[1];
        cv::Mat &v = *mats[2];

        Color yuv_color = rgb_to_yuv_color_table[rgb_color];
        cv::putText(y, text, pos, cv::FONT_HERSHEY_TRIPLEX, 1, yuv_color[0], 1);
        cv::Point2i pos_u_v(pos.x / 2, pos.y / 2);
        cv::putText(u, text, pos_u_v, cv::FONT_HERSHEY_TRIPLEX, 0.5, yuv_color[1], 1);
        cv::putText(v, text, pos_u_v, cv::FONT_HERSHEY_TRIPLEX, 0.5, yuv_color[2], 1);
    }
};
