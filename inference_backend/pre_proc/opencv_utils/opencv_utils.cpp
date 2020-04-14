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
        throw std::invalid_argument("Failed to create cv::Mat from image: unsupported image format");
    }
}

void NV12ImageToMats(const Image &src, cv::Mat &y, cv::Mat &uv) {
    if (src.format != FOURCC_NV12) {
        throw std::invalid_argument("Failed to create cv::Mat from image: unsupported image format");
    }
    y = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
    uv = cv::Mat(src.height / 2, src.width / 2, CV_8UC2, src.planes[1], src.stride[1]);
}

template <typename T>
void MatToMultiPlaneImageTyped(const cv::Mat &src, Image &dst) {
    ITT_TASK(__FUNCTION__);
    try {
        if (src.size().height < 0 or src.size().width < 0) {
            throw std::invalid_argument("Unsupported cv::Mat size");
        }
        const uint32_t src_height = static_cast<uint32_t>(src.size().height);
        const uint32_t src_width = static_cast<uint32_t>(src.size().width);

        if (src_height != dst.height or src_width != dst.width) {
            throw std::invalid_argument("Different height/width in ");
        }

        // This storage will used to
        uint32_t area = static_cast<uint32_t>(src.size().area());
        static std::vector<T> storage;
        if (area > storage.size()) {
            storage.resize(area);
        }

        int channels = src.channels();
        switch (channels) {
        case 3: {
            ITT_TASK("3-channel MatToMultiPlaneImage");
            std::vector<cv::Mat_<T>> mats(channels);
            mats[0] = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[0]));
            mats[1] = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[1]));
            mats[2] = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[2]));
            cv::split(src, mats);
            break;
        }
        case 4: {
            ITT_TASK("4-channel MatToMultiPlaneImage");
            std::vector<cv::Mat_<T>> mats(channels);
            mats[0] = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[0]));
            mats[1] = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[1]));
            mats[2] = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[2]));
            mats[3] = cv::Mat_<T>(dst.height, dst.width, storage.data());
            cv::split(src, mats);
            break;
        }
        default: {
            throw std::invalid_argument(
                "Failed to parse multi-plane image from cv::Mat: unsupported number of channels " +
                std::to_string(channels));
            break;
        }
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to transform one-plane cv::Mat to multi-plane cv::Mat"));
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
        throw std::invalid_argument(
            "Failed to parse multi-plane image from cv::Mat: unsupported image format (only U8 and F32 supported)");
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
