/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"
#include "inference_backend/input_image_layer_descriptor.h"
#include <opencv2/imgproc.hpp>

namespace InferenceBackend {

namespace Utils {

int ImageToMat(const Image &src, cv::Mat &dst);
void MatToMultiPlaneImage(const cv::Mat &mat, Image &dst);
cv::Mat ResizeMat(const cv::Mat &orig_image, const size_t height, const size_t width);

void ResizeAspectRatio(cv::Mat &image, const cv::Size &dst_size,
                       const ImageTransformationParams::Ptr &image_transform_info,
                       const size_t scale_param = 0); // scale_param: size + size//scale_parameter
void Resize(cv::Mat &image, const cv::Size &dst_size);
void Crop(cv::Mat &image, const cv::Rect &roi, const ImageTransformationParams::Ptr &image_transform_info);
void ColorSpaceConvert(const cv::Mat &orig_image, cv::Mat &result_img, const int src_color_format,
                       InputImageLayerDesc::ColorSpace target_color_format);
void Normalization(cv::Mat &image, double alpha, double beta);
void Normalization(cv::Mat &image, const std::vector<double> &alpha, const std::vector<double> &beta);

} // namespace Utils

} // namespace InferenceBackend
