/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "renderer.h"

class RendererBGR : public Renderer {
  protected:
    std::unordered_map<Color, Color> color_table;
    void convert_colors(const std::vector<cv::Scalar> &rgb_colors) {
        for (const Color &rgb_c : rgb_colors) {
            color_table[rgb_c] = Color(rgb_c[2], rgb_c[1], rgb_c[0]);
        }
    }

  public:
    RendererBGR(const std::vector<cv::Scalar> &rgb_colors) {
        convert_colors(rgb_colors);
    }
    void draw_rectangle(std::vector<std::shared_ptr<cv::Mat>> mats, Color rgb_color, cv::Point2i bbox_min,
                        cv::Point2i bbox_max) override {
        cv::Mat &mat = *mats[0];
        Color color = color_table[rgb_color];
        cv::rectangle(mat, bbox_min, bbox_max, color, 1);
    }
    void draw_circle(std::vector<std::shared_ptr<cv::Mat>> mats, Color rgb_color, cv::Point2i pos,
                     size_t thickness) override {
        cv::Mat &mat = *mats[0];
        Color color = color_table[rgb_color];
        cv::circle(mat, pos, thickness, color, -1);
    };
    void draw_text(std::vector<std::shared_ptr<cv::Mat>> mats, Color rgb_color, cv::Point2i pos,
                   const std::string &text) override {
        cv::Mat &mat = *mats[0];
        Color color = color_table[rgb_color];
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_TRIPLEX, 1, color, 1);
    };
};

class RendererRGB : public RendererBGR {
  protected:
    std::unordered_map<Color, Color> color_table;
    void convert_colors(const std::vector<cv::Scalar> &rgb_colors) {
        for (const Color &rgb_c : rgb_colors) {
            color_table[rgb_c] = rgb_c;
        }
    }

  public:
    RendererRGB(const std::vector<cv::Scalar> &rgb_colors) : RendererBGR(rgb_colors) {
    }
};
