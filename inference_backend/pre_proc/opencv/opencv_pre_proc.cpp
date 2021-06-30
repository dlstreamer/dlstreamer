/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_pre_proc.h"

#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"
#include "opencv_utils.h"

#include <opencv2/opencv.hpp>

using namespace InferenceBackend;
using namespace InferenceBackend::Utils;

ImagePreprocessor *InferenceBackend::CreatePreProcOpenCV() {
    return new OpenCV_VPP();
}

OpenCV_VPP::OpenCV_VPP() {
}

OpenCV_VPP::~OpenCV_VPP() {
}

namespace {

void CopyImage(const Image &src, Image &dst) {
    int planes_count = GetPlanesCount(src.format);
    for (int i = 0; i < planes_count; i++) {
        uint32_t dst_stride = src.width;
        uint32_t src_stride = (src.width == src.stride[i]) ? src.width : src.stride[i];
        for (uint32_t row = 0; row < src.height; ++row) {
            memcpy(dst.planes[i] + row * dst_stride, src.planes[i] + row * src_stride, dst_stride);
        }
        dst.stride[i] = dst_stride;
    }
}

} // namespace

cv::Mat CustomImageConvert(const cv::Mat &orig_image, const int src_color_format, const cv::Size &dst_size,
                           const InputImageLayerDesc::Ptr &pre_proc_info,
                           const ImageTransformationParams::Ptr &image_transform_info) {
    try {
        if (!pre_proc_info)
            throw std::runtime_error("Pre-processor info for custom image pre-processing is null.");

        cv::Mat result_img(orig_image.size(), orig_image.type());

        if (pre_proc_info->doNeedColorSpaceConversion(src_color_format)) {
            ColorSpaceConvert(orig_image, result_img, src_color_format, pre_proc_info->getTargetColorSpace());
        } else {
            orig_image.copyTo(result_img);
        }

        cv::Size resized_size = dst_size;
        if (pre_proc_info->doNeedPadding()) {
            const auto &padding = pre_proc_info->getPadding();
            resized_size.width -= padding.stride_x * 2;
            resized_size.height -= padding.stride_y * 2;
        }

        if (pre_proc_info->doNeedResize() and result_img.size() != resized_size) {
            switch (pre_proc_info->getResizeType()) {
            case InputImageLayerDesc::Resize::NO_ASPECT_RATIO:
                Resize(result_img, resized_size);
                break;
            case InputImageLayerDesc::Resize::ASPECT_RATIO:
                if (pre_proc_info->doNeedCrop()) { // resize to bigger size, because after we using Crop to dst size -
                                                   // standart practic
                    // scale_parameter: 1 + 1//scale_parameter
                    ResizeAspectRatio(result_img, resized_size, image_transform_info, 8);
                } else {
                    ResizeAspectRatio(result_img, resized_size, image_transform_info);
                }
                break;
            default:
                break;
            }
        }
        if (pre_proc_info->doNeedCrop() and result_img.size() != resized_size) {
            cv::Rect crop_roi;
            switch (pre_proc_info->getCropType()) {
            case InputImageLayerDesc::Crop::CENTRAL:
                crop_roi = cv::Rect((result_img.size().width - resized_size.width) / 2,
                                    (result_img.size().height - resized_size.height) / 2, resized_size.width,
                                    resized_size.height);
                break;
            case InputImageLayerDesc::Crop::TOP_LEFT:
                crop_roi = cv::Rect(0, 0, resized_size.width, resized_size.height);
                break;
            case InputImageLayerDesc::Crop::TOP_RIGHT:
                crop_roi =
                    cv::Rect(result_img.size().width - resized_size.width, 0, resized_size.width, resized_size.height);
                break;
            case InputImageLayerDesc::Crop::BOTTOM_LEFT:
                crop_roi = cv::Rect(0, result_img.size().height - resized_size.height, resized_size.width,
                                    resized_size.height);
                break;
            case InputImageLayerDesc::Crop::BOTTOM_RIGHT:
                crop_roi =
                    cv::Rect(result_img.size().width - resized_size.width,
                             result_img.size().height - resized_size.height, resized_size.width, resized_size.height);
                break;
            default:
                break;
            }
            Crop(result_img, crop_roi, image_transform_info);
        }

        if (pre_proc_info->doNeedRangeNormalization()) {
            const auto &range_norm = pre_proc_info->getRangeNormalization();
            const double std = 255.0 / (range_norm.max - range_norm.min);
            const double mean = 0 - range_norm.min;
            Normalization(result_img, mean, std);
        }
        if (pre_proc_info->doNeedDistribNormalization()) {
            const auto &distrib_norm = pre_proc_info->getDistribNormalization();
            Normalization(result_img, distrib_norm.mean, distrib_norm.std);
        }

        if (pre_proc_info->doNeedPadding()) {
            const auto &padding = pre_proc_info->getPadding();
            AddPadding(result_img, dst_size, padding.stride_x, padding.stride_y, padding.fill_value,
                       image_transform_info);
        }

        return result_img;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed custom image pre-processing."));
    }
    return cv::Mat();
}

void OpenCV_VPP::Convert(const Image &raw_src, Image &dst, const InputImageLayerDesc::Ptr &pre_proc_info,
                         const ImageTransformationParams::Ptr &image_transform_info, bool bAllocateDestination) {
    ITT_TASK("OpenCV_VPP");

    try {
        if (bAllocateDestination) {
            throw std::invalid_argument("bAllocateDestination set to true is not supported");
        }

        Image src = ApplyCrop(raw_src);
        // if identical format and resolution
        if (doNeedPreProcessing(raw_src, dst)) {
            CopyImage(raw_src, dst);
            // do not return here. Code below is mandatory to execute landmarks inference on CentOS in case of vaapi
            // pre-proc
        }

        cv::Mat src_mat_image;
        cv::Mat dst_mat_image;

        auto converted_format = ImageToMat(src, src_mat_image);

        if (doNeedCustomImageConvert(pre_proc_info)) {
            if (dst.height > static_cast<uint32_t>(std::numeric_limits<int>::max()) and
                dst.width > static_cast<uint32_t>(std::numeric_limits<int>::max()))
                throw std::runtime_error("Image size too large.");

            cv::Size dst_size(static_cast<int>(dst.width), static_cast<int>(dst.height));
            dst_mat_image =
                CustomImageConvert(src_mat_image, converted_format, dst_size, pre_proc_info, image_transform_info);
        } else {
            dst_mat_image = ResizeMat(src_mat_image, dst.height, dst.width);
        }

        MatToMultiPlaneImage(dst_mat_image, dst);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during OpenCV image pre-processing"));
    }
}

void OpenCV_VPP::ReleaseImage(const Image &) {
}
