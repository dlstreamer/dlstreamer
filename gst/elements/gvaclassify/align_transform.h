/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __ALIGN_TRANSFORM_H__
#define __ALIGN_TRANSFORM_H__

#include <opencv2/imgproc.hpp>

cv::Mat GetTransform(cv::Mat *src, cv::Mat *dst);
void align_rgb_image(InferenceBackend::Image &image, const std::vector<float> &landmarks_points,
                     const std::vector<float> &reference_points);

#endif /* __ALIGN_TRANSFORM_H__ */
