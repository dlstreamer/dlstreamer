/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"
#include <opencv2/imgproc.hpp>

namespace InferenceBackend {

namespace Utils {

// TODO: add I420 support
void ImageToMat(const Image &src, cv::Mat &dst);
void NV12ImageToMats(const Image &src, cv::Mat &y, cv::Mat &uv);
void MatToMultiPlaneImage(const cv::Mat &mat, Image &dst);
cv::Mat ResizeMat(const cv::Mat &orig_image, const size_t height, const size_t width);
} // namespace Utils
} // namespace InferenceBackend
