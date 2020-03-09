/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_utils.h"
#include "inference_backend/logger.h"

#include <opencv2/opencv.hpp>

namespace InferenceBackend {

namespace Utils {

void ImageToMat(const Image &src, cv::Mat &dst) {
    switch (src.format) {
    case FOURCC_BGRX:
    case FOURCC_BGRA:
        dst = cv::Mat(src.height, src.width, CV_8UC4, src.planes[0], src.stride[0]);
        break;
    case FOURCC_BGR:
        dst = cv::Mat(src.height, src.width, CV_8UC3, src.planes[0], src.stride[0]);
        break;
    case FOURCC_BGRP: {
        cv::Mat channelB = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
        cv::Mat channelG = cv::Mat(src.height, src.width, CV_8UC1, src.planes[1], src.stride[1]);
        cv::Mat channelR = cv::Mat(src.height, src.width, CV_8UC1, src.planes[2], src.stride[2]);
        std::vector<cv::Mat> channels{channelB, channelG, channelR};
        cv::merge(channels, dst);
        break;
    }
    case FOURCC_RGBP: {
        cv::Mat channelR = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
        cv::Mat channelG = cv::Mat(src.height, src.width, CV_8UC1, src.planes[1], src.stride[1]);
        cv::Mat channelB = cv::Mat(src.height, src.width, CV_8UC1, src.planes[2], src.stride[2]);
        std::vector<cv::Mat> channels{channelB, channelG, channelR};
        cv::merge(channels, dst);
        break;
    }
    default:
        throw std::runtime_error("ImageToMat: Unsupported format in opencv pre-proc");
    }
}

void NV12ImageToMats(const Image &src, cv::Mat &y, cv::Mat &uv) {
    if (src.format != FOURCC_NV12) {
        throw std::runtime_error("NV12ImageToMats: Unsupported format");
    }
    y = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
    uv = cv::Mat(src.height / 2, src.width / 2, CV_8UC2, src.planes[1], src.stride[1]);
}

template <typename T>
void MatToMultiPlaneImageTyped(const cv::Mat &src, Image &dst) {
    ITT_TASK(__FUNCTION__);
    const size_t width = dst.width;
    const size_t height = dst.height;
    const size_t channels = 3;
    T *dst_data = (T *)dst.planes[0];

    if (src.channels() == 4) {
        ITT_TASK("4-channel MatToMultiPlaneImageTyped");
        for (size_t c = 0; c < channels; c++) {
            for (size_t h = 0; h < height; h++) {
                for (size_t w = 0; w < width; w++) {
                    dst_data[c * width * height + h * width + w] = src.at<cv::Vec4b>(h, w)[c];
                }
            }
        }
    } else if (src.channels() == 3) {
        ITT_TASK("3-channel MatToMultiPlaneImageTyped");
        for (size_t c = 0; c < channels; c++) {
            for (size_t h = 0; h < height; h++) {
                for (size_t w = 0; w < width; w++) {
                    dst_data[c * width * height + h * width + w] = src.at<cv::Vec3b>(h, w)[c];
                }
            }
        }
    } else {
        throw std::runtime_error("Image with unsupported channels number");
    }
}

void MatToMultiPlaneImage(const cv::Mat &src, Image &dst) {
    switch (dst.format) {
    case FOURCC_RGBP:
        MatToMultiPlaneImageTyped<uint8_t>(src, dst);
        break;
    case FOURCC_RGBP_F32:
        MatToMultiPlaneImageTyped<float>(src, dst);
        break;
    default:
        throw std::runtime_error("BGRToImage: Can not convert to desired format. Not implemented");
    }
}

cv::Mat ResizeMat(const cv::Mat &orig_image, const size_t height, const size_t width) {
    cv::Mat resized_image(orig_image);
    if (width != (size_t)orig_image.size().width || height != (size_t)orig_image.size().height) {
        ITT_TASK("cv::resize");
        cv::resize(orig_image, resized_image, cv::Size(width, height));
    }
    return resized_image;
}
} // namespace Utils
} // namespace InferenceBackend
