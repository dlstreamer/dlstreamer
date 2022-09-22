/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/gst.h>
#include <opencv2/opencv.hpp>

void drawAxes(cv::Mat &frame, cv::Point3f cpoint, double yaw, double pitch, double roll, float scale);
