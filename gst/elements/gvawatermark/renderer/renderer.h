/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <opencv2/opencv.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#ifdef __linux__
#include <unistd.h>
#endif
#include <unordered_map>
#include <vector>

using Color = cv::Scalar;

namespace std {

template <>
struct hash<Color> {
    std::size_t operator()(const Color &color) const {
        std::string s = std::to_string(color[0]) + std::to_string(color[1]) + std::to_string(color[2]);
        return std::hash<std::string>()(s);
    }
};

} // namespace std

class Renderer {
  public:
    virtual void draw_rectangle(std::vector<std::shared_ptr<cv::Mat>>, Color, cv::Point2i, cv::Point2i) = 0;
    virtual void draw_circle(std::vector<std::shared_ptr<cv::Mat>>, Color, cv::Point2i, size_t) = 0;
    virtual void draw_text(std::vector<std::shared_ptr<cv::Mat>>, Color, cv::Point2i, const std::string &label) = 0;
    virtual ~Renderer() = default;
};

class RendererYUV : public Renderer {
  protected:
    void fill_color_conversion_matrix(double Kr, double Kb, double (&matrix)[3][3]) {
        double Kg = 1.0 - Kr - Kb;
        double k1 = 1 - Kb;
        double k2 = 1 - Kr;

        matrix[0][0] = Kr;
        matrix[0][1] = Kg;
        matrix[0][2] = Kb;

        matrix[1][0] = -Kr / (2 * k1);
        matrix[1][1] = -Kg / (2 * k1);
        matrix[1][2] = 1. / 2;

        matrix[2][0] = 1. / 2;
        matrix[2][1] = -Kg / (2 * k2);
        matrix[2][2] = -Kb / (2 * k2);
    }

    Color convert_color(const Color &c, double (&matrix)[3][3]) {
        Color x;
        x[0] = CLAMP(matrix[0][0] * c[0] + matrix[0][1] * c[1] + matrix[0][2] * c[2], 0, 255);
        x[1] = CLAMP(matrix[1][0] * c[0] + matrix[1][1] * c[1] + matrix[1][2] * c[2] + 128, 0, 255);
        x[2] = CLAMP(matrix[2][0] * c[0] + matrix[2][1] * c[1] + matrix[2][2] * c[2] + 128, 0, 255);
        return x;
    }

    void convert_colors_rgb_to_yuv(double Kr, double Kb, const std::vector<Color> &rgb_colors,
                                   std::unordered_map<Color, Color> &yuv_colors, double (&matrix)[3][3]) {
        fill_color_conversion_matrix(Kr, Kb, matrix);
        for (const Color &rgb_c : rgb_colors) {
            yuv_colors[rgb_c] = convert_color(rgb_c, matrix);
        }
    }
    double coefficient_matrix[3][3];
    std::unordered_map<Color, Color> rgb_to_yuv_color_table;

    template <int n>
    void check_planes(std::vector<std::shared_ptr<cv::Mat>> p) {
        if (p.size() != n) {
            throw std::runtime_error("plane count != " + std::to_string(n));
        }
    }
};
