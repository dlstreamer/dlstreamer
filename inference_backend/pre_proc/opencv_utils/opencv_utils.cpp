/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_utils.h"
#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"

#include <opencv2/opencv.hpp>

namespace InferenceBackend {

namespace Utils {

int ImageToMat(const Image &src, cv::Mat &dst) {
    switch (src.format) {
    case FOURCC_BGRX:
    case FOURCC_BGRA:
        dst = cv::Mat(src.height, src.width, CV_8UC4, src.planes[0], src.stride[0]);
        return FOURCC_BGRA;
    case FOURCC_BGR:
        dst = cv::Mat(src.height, src.width, CV_8UC3, src.planes[0], src.stride[0]);
        return FOURCC_BGR;
    case FOURCC_BGRP: {
        cv::Mat channelB = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
        cv::Mat channelG = cv::Mat(src.height, src.width, CV_8UC1, src.planes[1], src.stride[1]);
        cv::Mat channelR = cv::Mat(src.height, src.width, CV_8UC1, src.planes[2], src.stride[2]);
        std::vector<cv::Mat> channels{channelB, channelG, channelR};
        cv::merge(channels, dst);
        return FOURCC_BGRP;
    }
    case FOURCC_RGBP: {
        cv::Mat channelR = cv::Mat(src.height, src.width, CV_8UC1, src.planes[0], src.stride[0]);
        cv::Mat channelG = cv::Mat(src.height, src.width, CV_8UC1, src.planes[1], src.stride[1]);
        cv::Mat channelB = cv::Mat(src.height, src.width, CV_8UC1, src.planes[2], src.stride[2]);
        std::vector<cv::Mat> channels{channelB, channelG, channelR};
        cv::merge(channels, dst);
        return FOURCC_BGRP;
    }
    case FOURCC_I420: {
        const uint32_t height = (src.height % 2 == 0) ? src.height : src.height - 1;
        const uint32_t width = (src.width % 2 == 0) ? src.width : src.width - 1;
        const uint32_t size = height * width;

        const uint32_t half_height = src.height / 2;
        const uint32_t half_width = src.width / 2;
        const uint32_t quarter_size = half_height * half_width;

        cv::Mat yuv420;
        if (src.planes[1] == (src.planes[0] + size) and src.planes[2] == (src.planes[1] + quarter_size)) {
            // If image is provided by libav decoder, YUV planes are stored sequentially with zero strides
            yuv420 = cv::Mat(height + half_height, width, CV_8UC1, src.planes[0]);
        } else {
            // If image is provided by vaapi decoder/postprocessing, YUV planes are stored with non-zero strides
            yuv420 = cv::Mat(height + half_height, width, CV_8UC1);

            cv::Mat raw_y = cv::Mat(height, width, CV_8UC1, src.planes[0], src.stride[0]);
            cv::Mat y = cv::Mat(height, width, CV_8UC1, yuv420.data);
            raw_y.copyTo(y);

            cv::Mat raw_u = cv::Mat(half_height, half_width, CV_8UC1, src.planes[1], src.stride[1]);
            cv::Mat u = cv::Mat(half_height, half_width, CV_8UC1, yuv420.data + size);
            raw_u.copyTo(u);

            cv::Mat raw_v = cv::Mat(half_height, half_width, CV_8UC1, src.planes[2], src.stride[2]);
            cv::Mat v = cv::Mat(half_height, half_width, CV_8UC1, yuv420.data + size + quarter_size);
            raw_v.copyTo(v);
        }
        cv::cvtColor(yuv420, dst, cv::COLOR_YUV2BGR_I420);
        return FOURCC_BGR;
    }
    case FOURCC_NV12: {
        const uint32_t height = (src.height % 2 == 0) ? src.height : src.height - 1;
        const uint32_t width = (src.width % 2 == 0) ? src.width : src.width - 1;
        const uint32_t size = height * width;

        const uint32_t half_height = height / 2;
        const uint32_t half_width = width / 2;

        cv::Mat yuv12 = cv::Mat(height + half_height, width, CV_8UC1);

        cv::Mat raw_y = cv::Mat(height, width, CV_8UC1, src.planes[0], src.stride[0]);
        cv::Mat y12 = cv::Mat(height, width, CV_8UC1, yuv12.data);
        raw_y.copyTo(y12);

        cv::Mat raw_uv = cv::Mat(half_height, half_width, CV_8UC2, src.planes[1], src.stride[1]);
        cv::Mat uv12 = cv::Mat(half_height, half_width, CV_8UC2, yuv12.data + size);
        raw_uv.copyTo(uv12);

        cv::cvtColor(yuv12, dst, cv::COLOR_YUV2BGR_NV12);
        return FOURCC_BGR;
    }
    default:
        throw std::invalid_argument("Failed to create cv::Mat from image: unsupported image format.");
    }
}

template <typename T>
void MatToMultiPlaneImageTyped(const cv::Mat &src, Image &dst) {
    ITT_TASK(__FUNCTION__);
    try {
        if (src.size().height < 0 or src.size().width < 0) {
            throw std::invalid_argument("Unsupported cv::Mat size.");
        }
        const uint32_t src_height = static_cast<uint32_t>(src.size().height);
        const uint32_t src_width = static_cast<uint32_t>(src.size().width);

        if (src_height != dst.height or src_width != dst.width) {
            throw std::invalid_argument("MatToMultiPlaneImageTyped: Different height/width in cv::Mat and Image.");
        }

        // This storage will used to
        uint32_t area = static_cast<uint32_t>(src.size().area());
        static std::vector<T> storage;
        if (area > storage.size()) {
            storage.resize(area);
        }

        int channels = src.channels();
        switch (channels) {
        case 1: {
            ITT_TASK("1-channel MatToMultiPlaneImage");
            cv::Mat_<T> wrapped_mat = cv::Mat_<T>(dst.height, dst.width, reinterpret_cast<T *>(dst.planes[0]));
            src.copyTo(wrapped_mat);
            break;
        }
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
        std::throw_with_nested(std::runtime_error("Failed to transform one-plane cv::Mat to multi-plane cv::Mat."));
    }
}

void MatToMultiPlaneImage(const cv::Mat &src, Image &dst) {
    switch (dst.format) {
    case FOURCC_RGBP: {
        if (src.depth() != CV_8U)
            throw std::runtime_error("Image\'s depth should be CV_8U.");

        MatToMultiPlaneImageTyped<uint8_t>(src, dst);
        break;
    }
    case FOURCC_RGBP_F32: {
        if (src.depth() != CV_32F)
            throw std::runtime_error("Image\'s depth should be CV_FP32.");

        MatToMultiPlaneImageTyped<float>(src, dst);
        break;
    }
    default:
        throw std::invalid_argument(
            "Failed to parse multi-plane image from cv::Mat: unsupported image format (only U8 and F32 supported).");
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

void ResizeAspectRatio(cv::Mat &image, const cv::Size &dst_size,
                       const ImageTransformationParams::Ptr &image_transform_info, const size_t scale_param) {
    try {
        if (dst_size == image.size())
            return;

        ITT_TASK("ResizeAspectRatio");
        cv::Size target_dst_size(dst_size.width, dst_size.height);
        if (scale_param) {
            target_dst_size.width += dst_size.width / scale_param;
            target_dst_size.height += dst_size.height / scale_param;
        }
        auto orig_width = image.size().width;
        auto orig_height = image.size().height;
        double scale = std::min(static_cast<double>(target_dst_size.width) / orig_width,
                                static_cast<double>(target_dst_size.height) / orig_height);
        auto width = orig_width * scale;
        auto height = orig_height * scale;

        cv::Mat resized_image;
        cv::resize(image, resized_image, cv::Size(width, height));

        // background
        image = cv::Mat(target_dst_size, image.type(), {128, 128, 128});
        cv::Rect place_to_insert((target_dst_size.width - width) / 2, (target_dst_size.height - height) / 2, width,
                                 height);
        cv::Mat insertPos(image, place_to_insert);
        // inplace insertion
        resized_image.copyTo(insertPos);

        // need for post-processing
        image_transform_info->AspectRatioResizeHasDone(place_to_insert.x, place_to_insert.y, scale, scale);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during ResizeAspectRatio image pre-processing."));
    }
}

void Resize(cv::Mat &image, const cv::Size &dst_size) {
    try {
        if (dst_size == image.size())
            return;
        ITT_TASK("cv::resize");
        cv::resize(image, image, dst_size);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during Resize image pre-processing."));
    }
} // namespace Utils

void Crop(cv::Mat &image, const cv::Rect &roi, const ImageTransformationParams::Ptr &image_transform_info) {
    try {
        if (roi.size() == image.size())
            return;

        ITT_TASK("Crop");
        image = image(roi);

        // need for post-processing
        image_transform_info->CropHasDone(roi.x, roi.y);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during Crop image pre-processing"));
    }
}

void Normalization(cv::Mat &image, double mean, double std) {
    ITT_TASK("cv::convertTo");
    image.convertTo(image, CV_MAKETYPE(CV_32F, image.channels()), (1 / std), (0 - mean));
}

void Normalization(cv::Mat &image, const std::vector<double> &mean, const std::vector<double> &std) {
    ITT_TASK(__FUNCTION__);
    assert(std.size() == mean.size());
    const size_t channels_num = safe_convert<size_t>(image.channels());
    if (channels_num != mean.size())
        throw std::runtime_error("Image\'s channels number does not match with size of mean/std parameters.");

    if (image.depth() != CV_32F)
        image.convertTo(image, CV_MAKETYPE(CV_32F, image.channels()));

    switch (channels_num) {
    case 1:
        image.forEach<cv::Vec<float, 1>>([&mean, &std](cv::Vec<float, 1> &pixel, const int *position) -> void {
            (void)(position);

            pixel[0] = (pixel[0] - mean[0]) / std[0];
        });
        break;
    case 3:
        image.forEach<cv::Vec<float, 3>>([&mean, &std](cv::Vec<float, 3> &pixel, const int *position) -> void {
            (void)(position);

            pixel[0] = (pixel[0] - mean[0]) / std[0];
            pixel[1] = (pixel[1] - mean[1]) / std[1];
            pixel[2] = (pixel[2] - mean[2]) / std[2];
        });
        break;
    case 4:
        image.forEach<cv::Vec<float, 4>>([&mean, &std](cv::Vec<float, 4> &pixel, const int *position) -> void {
            (void)(position);

            pixel[0] = (pixel[0] - mean[0]) / std[0];
            pixel[1] = (pixel[1] - mean[1]) / std[1];
            pixel[2] = (pixel[2] - mean[2]) / std[2];
            pixel[3] = (pixel[3] - mean[3]) / std[3];
        });
        break;

    default:
        throw std::runtime_error("Unsupported image channels number.");
    }
}

void ColorSpaceConvert(const cv::Mat &orig_image, cv::Mat &result_img, const int src_color_format,
                       InputImageLayerDesc::ColorSpace target_color_format) {
    try {
        switch (target_color_format) {
        case InputImageLayerDesc::ColorSpace::BGR:
            switch (src_color_format) {
            case FOURCC_RGB:
                cv::cvtColor(orig_image, result_img, cv::COLOR_RGB2BGR);
                break;
            case FOURCC_RGBA:
            case FOURCC_RGBX:
                cv::cvtColor(orig_image, result_img, cv::COLOR_RGBA2BGR);
                break;
            case FOURCC_BGRA:
            case FOURCC_BGRX:
                cv::cvtColor(orig_image, result_img, cv::COLOR_BGRA2BGR);
                break;
            default:
                throw std::runtime_error("Color-space conversion for your format has not been implemented yet.");
                break;
            }
            break;
        case InputImageLayerDesc::ColorSpace::RGB:
            switch (src_color_format) {
            case FOURCC_BGR:
                cv::cvtColor(orig_image, result_img, cv::COLOR_BGR2RGB);
                break;
            case FOURCC_RGBA:
            case FOURCC_RGBX:
                cv::cvtColor(orig_image, result_img, cv::COLOR_RGBA2RGB);
                break;
            case FOURCC_BGRA:
            case FOURCC_BGRX:
                cv::cvtColor(orig_image, result_img, cv::COLOR_BGRA2RGB);
                break;
            default:
                throw std::runtime_error("Color-space conversion for your format has not been implemented yet.");
                break;
            }
            break;
        case InputImageLayerDesc::ColorSpace::GRAYSCALE:
            switch (src_color_format) {
            case FOURCC_BGR:
                cv::cvtColor(orig_image, result_img, cv::COLOR_BGR2GRAY);
                break;
            case FOURCC_RGBA:
            case FOURCC_RGBX:
                cv::cvtColor(orig_image, result_img, cv::COLOR_RGBA2GRAY);
                break;
            case FOURCC_BGRA:
            case FOURCC_BGRX:
                cv::cvtColor(orig_image, result_img, cv::COLOR_BGRA2GRAY);
                break;
            default:
                throw std::runtime_error("Color-space conversion for your format has not been implemented yet.");
                break;
            }
            break;
        case InputImageLayerDesc::ColorSpace::YUV:
            throw std::runtime_error("Color-space conversion to YUV has not been implemented yet.");
            break;
        default:
            throw std::runtime_error("Color-space conversion for your format has not been implemented yet.");
            break;
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during ColorSpaceConvert image pre-processing."));
    }
}

} // namespace Utils

} // namespace InferenceBackend
