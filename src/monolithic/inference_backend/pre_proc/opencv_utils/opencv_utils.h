/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"
#include "inference_backend/input_image_layer_descriptor.h"

#include <opencv2/imgproc.hpp>

namespace InferenceBackend {

namespace Utils {

int CreateMat(uint8_t *const *planes, uint32_t width, uint32_t height, int format, const uint32_t *stride,
              const uint32_t *offset, cv::Mat &dst);
int ImageToMat(const Image &src, cv::Mat &dst);

void MatToMultiPlaneImage(const cv::Mat &mat, Image &dst);
void MatToMultiPlaneImage(const cv::Mat &mat, int dst_format, uint32_t dst_width, uint32_t dst_height,
                          uint8_t *const *dst_planes);

cv::Mat ResizeMat(const cv::Mat &orig_image, const size_t height, const size_t width);

/**
 * @brief Resize image to dst_size preserving aspect ratio
 * @param strict If true then resize exactly to dst_size with adding of background if needed.
 * Otherwise resize to min dimension in dst_size
 * @param scale_param If not 0 target size calculated as 'size + size // scale_param'
 */
void ResizeAspectRatio(cv::Mat &image, const cv::Size &dst_size,
                       const ImageTransformationParams::Ptr &image_transform_info, const size_t scale_param = 0,
                       bool strict = true);

void Resize(cv::Mat &image, const cv::Size &dst_size);

void Crop(cv::Mat &image, const cv::Rect &roi, const ImageTransformationParams::Ptr &image_transform_info);

void AddPadding(cv::Mat &image, const cv::Size dst_size, size_t stride_x, size_t stride_y,
                const std::vector<double> &fill_value, const ImageTransformationParams::Ptr &image_transform_info);

void ColorSpaceConvert(const cv::Mat &orig_image, cv::Mat &result_img, const int src_color_format,
                       InputImageLayerDesc::ColorSpace target_color_format);

void Normalization(cv::Mat &image, double alpha, double beta);
void Normalization(cv::Mat &image, const std::vector<double> &alpha, const std::vector<double> &beta);

} // namespace Utils

} // namespace InferenceBackend
