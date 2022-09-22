/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <string>
#include <variant>

#include <opencv2/opencv.hpp>

namespace render {

struct Text {
    std::string text;
    cv::Point org;
    int fonttype;
    double fontscale;
    cv::Scalar color;
    int thick;

    Text() = default;

    Text(const std::string &text, const cv::Point &org, int fonttype, double fontscale, const cv::Scalar &color,
         int thick = 1)
        : text(text), org(org), fonttype(fonttype), fontscale(fontscale), color(color), thick(thick) {
    }
};

struct Rect {
    cv::Rect rect;
    cv::Scalar color;
    int thick;

    Rect() = default;

    Rect(const cv::Rect &rect, const cv::Scalar &color, int thick = 1) : rect(rect), color(color), thick(thick) {
    }
};

struct Circle {
    cv::Point center;
    int radius;
    cv::Scalar color;
    int thick;

    Circle() = default;

    Circle(const cv::Point &center, int radius, const cv::Scalar &color, int thick = 1)
        : center(center), radius(radius), color(color), thick(thick) {
    }
};

struct Line {
    cv::Point pt1;
    cv::Point pt2;
    cv::Scalar color;
    int thick;

    Line() = default;

    Line(const cv::Point &pt1, const cv::Point &pt2, const cv::Scalar &color, int thick = 1)
        : pt1(pt1), pt2(pt2), color(color), thick(thick) {
    }
};

using Prim = std::variant<Text, Rect, Circle, Line>;

} // namespace render
