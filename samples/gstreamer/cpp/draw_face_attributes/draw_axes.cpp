/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 * Reused 2 functions from interactive_face_detection_demo
 * See https://github.com/openvinotoolkit/open_model_zoo/blob/2018/demos/interactive_face_detection_demo
 * Changed argument list for HeadPoseDetection::drawAxes
 * Added return value for HeadPoseDetection::buildCameraMatrix
 * Adapted code style to match with Video Analytics GStreamer* plugins project
 * Fixed warnings
 ******************************************************************************/

#include "draw_axes.h"

cv::Mat buildCameraMatrix(int cx, int cy, float focalLength) {
    cv::Mat cameraMatrix = cv::Mat::zeros(3, 3, CV_32F);
    cameraMatrix.at<float>(0) = focalLength;
    cameraMatrix.at<float>(2) = static_cast<float>(cx);
    cameraMatrix.at<float>(4) = focalLength;
    cameraMatrix.at<float>(5) = static_cast<float>(cy);
    cameraMatrix.at<float>(8) = 1;
    return cameraMatrix;
}

void drawAxes(cv::Mat &frame, cv::Point3f cpoint, double yaw, double pitch, double roll, float scale) {
    pitch *= CV_PI / 180.0;
    yaw *= CV_PI / 180.0;
    roll *= CV_PI / 180.0;

    cv::Matx33f Rx(1, 0, 0, 0, cos(pitch), -sin(pitch), 0, sin(pitch), cos(pitch));
    cv::Matx33f Ry(cos(yaw), 0, -sin(yaw), 0, 1, 0, sin(yaw), 0, cos(yaw));
    cv::Matx33f Rz(cos(roll), -sin(roll), 0, sin(roll), cos(roll), 0, 0, 0, 1);

    auto r = cv::Mat(Rz * Ry * Rx);
    cv::Mat cameraMatrix = buildCameraMatrix(frame.cols / 2, frame.rows / 2, 950.0);

    cv::Mat xAxis(3, 1, CV_32F), yAxis(3, 1, CV_32F), zAxis(3, 1, CV_32F), zAxis1(3, 1, CV_32F);

    xAxis.at<float>(0) = 1 * scale;
    xAxis.at<float>(1) = 0;
    xAxis.at<float>(2) = 0;

    yAxis.at<float>(0) = 0;
    yAxis.at<float>(1) = -1 * scale;
    yAxis.at<float>(2) = 0;

    zAxis.at<float>(0) = 0;
    zAxis.at<float>(1) = 0;
    zAxis.at<float>(2) = -1 * scale;

    zAxis1.at<float>(0) = 0;
    zAxis1.at<float>(1) = 0;
    zAxis1.at<float>(2) = 1 * scale;

    cv::Mat o(3, 1, CV_32F, cv::Scalar(0));
    o.at<float>(2) = cameraMatrix.at<float>(0);

    xAxis = r * xAxis + o;
    yAxis = r * yAxis + o;
    zAxis = r * zAxis + o;
    zAxis1 = r * zAxis1 + o;

    cv::Point p1, p2;

    p2.x = static_cast<int>((xAxis.at<float>(0) / xAxis.at<float>(2) * cameraMatrix.at<float>(0)) + cpoint.x);
    p2.y = static_cast<int>((xAxis.at<float>(1) / xAxis.at<float>(2) * cameraMatrix.at<float>(4)) + cpoint.y);
    cv::line(frame, cv::Point(cpoint.x, cpoint.y), p2, cv::Scalar(0, 0, 255), 2);

    p2.x = static_cast<int>((yAxis.at<float>(0) / yAxis.at<float>(2) * cameraMatrix.at<float>(0)) + cpoint.x);
    p2.y = static_cast<int>((yAxis.at<float>(1) / yAxis.at<float>(2) * cameraMatrix.at<float>(4)) + cpoint.y);
    cv::line(frame, cv::Point(cpoint.x, cpoint.y), p2, cv::Scalar(0, 255, 0), 2);

    p1.x = static_cast<int>((zAxis1.at<float>(0) / zAxis1.at<float>(2) * cameraMatrix.at<float>(0)) + cpoint.x);
    p1.y = static_cast<int>((zAxis1.at<float>(1) / zAxis1.at<float>(2) * cameraMatrix.at<float>(4)) + cpoint.y);

    p2.x = static_cast<int>((zAxis.at<float>(0) / zAxis.at<float>(2) * cameraMatrix.at<float>(0)) + cpoint.x);
    p2.y = static_cast<int>((zAxis.at<float>(1) / zAxis.at<float>(2) * cameraMatrix.at<float>(4)) + cpoint.y);
    cv::line(frame, p1, p2, cv::Scalar(255, 0, 0), 2);
    cv::circle(frame, p2, 3, cv::Scalar(255, 0, 0), 2);
}
