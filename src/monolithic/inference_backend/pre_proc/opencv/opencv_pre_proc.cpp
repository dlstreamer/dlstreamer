/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_pre_proc.h"

#include "inference_backend/logger.h"
#include "opencv_utils.h"
#include "safe_arithmetic.hpp"
#include "utils.h"

#include <opencv2/opencv.hpp>

namespace GlobUtils = Utils;
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
    uint32_t planes_count = ::Utils::GetPlanesCount(src.format);
    for (uint32_t i = 0; i < planes_count; i++) {
        uint32_t dst_stride = src.width;
        uint32_t src_stride = (src.width == src.stride[i]) ? src.width : src.stride[i];
        for (uint32_t row = 0; row < src.height; ++row) {
            memcpy(dst.planes[i] + row * dst_stride, src.planes[i] + row * src_stride, dst_stride);
        }
        dst.stride[i] = dst_stride;
    }
}

} // namespace

cv::Mat CustomImageConvert(const cv::Mat &orig_image, const int src_color_format, const cv::Size &input_size,
                           const InputImageLayerDesc::Ptr &pre_proc_info,
                           const ImageTransformationParams::Ptr &image_transform_info) {
    try {
        if (!pre_proc_info)
            throw std::runtime_error("Pre-processor info for custom image pre-processing is null.");

        // Padding
        int padding_x = 0;
        int padding_y = 0;
        std::vector<double> fill_value;
        if (pre_proc_info->doNeedPadding()) {
            const auto &padding = pre_proc_info->getPadding();
            padding_x = safe_convert<int>(padding.stride_x);
            padding_y = safe_convert<int>(padding.stride_y);
            fill_value = padding.fill_value;
        }

        cv::Size input_size_except_padding(input_size.width - (padding_x * 2), input_size.height - (padding_y * 2));

        // Resize
        cv::Mat image_to_insert = orig_image;
        if (pre_proc_info->doNeedResize() && orig_image.size() != input_size_except_padding) {
            double additional_crop_scale_param = 1;
            if (pre_proc_info->doNeedCrop() && pre_proc_info->doNeedResize()) {
                additional_crop_scale_param = 1.125;
            }

            double resize_scale_param_x = 1;
            double resize_scale_param_y = 1;

            resize_scale_param_x = safe_convert<double>(input_size_except_padding.width) / orig_image.size().width;
            resize_scale_param_y = safe_convert<double>(input_size_except_padding.height) / orig_image.size().height;

            if (pre_proc_info->getResizeType() == InputImageLayerDesc::Resize::ASPECT_RATIO) {
                resize_scale_param_x = resize_scale_param_y = std::min(resize_scale_param_x, resize_scale_param_y);
            }

            resize_scale_param_x *= additional_crop_scale_param;
            resize_scale_param_y *= additional_crop_scale_param;

            cv::Size after_resize(orig_image.size().width * resize_scale_param_x,
                                  orig_image.size().height * resize_scale_param_y);

            ITT_TASK("cv::resize");
            cv::resize(orig_image, image_to_insert, after_resize);

            if (image_transform_info)
                image_transform_info->ResizeHasDone(resize_scale_param_x, resize_scale_param_y);
        }

        // Crop
        if (pre_proc_info->doNeedCrop() && image_to_insert.size() != input_size_except_padding) {
            size_t cropped_border_x = 0;
            size_t cropped_border_y = 0;
            if (image_to_insert.size().width > input_size_except_padding.width)
                cropped_border_x =
                    safe_convert<size_t>(safe_add(safe_convert<int64_t>(image_to_insert.size().width),
                                                  0 - safe_convert<int64_t>(input_size_except_padding.width)));
            if (image_to_insert.size().height > input_size_except_padding.height)
                cropped_border_y =
                    safe_convert<size_t>(safe_add(safe_convert<int64_t>(image_to_insert.size().height),
                                                  0 - safe_convert<int64_t>(input_size_except_padding.height)));

            cv::Size crop_rect_size(image_to_insert.size().width - safe_convert<int>(cropped_border_x),
                                    image_to_insert.size().height - safe_convert<int>(cropped_border_y));
            cv::Point2f top_left_rect_point;
            switch (pre_proc_info->getCropType()) {
            case InputImageLayerDesc::Crop::CENTRAL:
                top_left_rect_point = cv::Point2f(cropped_border_x / 2, cropped_border_y / 2);
                break;
            case InputImageLayerDesc::Crop::TOP_LEFT:
                top_left_rect_point = cv::Point2f(0, 0);
                break;
            case InputImageLayerDesc::Crop::TOP_RIGHT:
                top_left_rect_point = cv::Point2f(cropped_border_x, 0);
                break;
            case InputImageLayerDesc::Crop::BOTTOM_LEFT:
                top_left_rect_point = cv::Point2f(0, cropped_border_y);
                break;
            case InputImageLayerDesc::Crop::BOTTOM_RIGHT:
                top_left_rect_point = cv::Point2f(cropped_border_x, cropped_border_y);
                break;
            default:
                throw std::runtime_error("Unknown crop format.");
            }

            cv::Rect crop_rect(top_left_rect_point, crop_rect_size);
            Crop(image_to_insert, crop_rect, image_transform_info);
        }

        // Color Space Conversion
        if (pre_proc_info->doNeedColorSpaceConversion(src_color_format)) {
            ColorSpaceConvert(image_to_insert, image_to_insert, src_color_format, pre_proc_info->getTargetColorSpace());
        }

        // Normalization is handled in OV backend, as all input images are assumed to be U8
        // if (pre_proc_info->doNeedRangeNormalization()) {
        //     const auto &range_norm = pre_proc_info->getRangeNormalization();
        //     const double std = 255.0 / (range_norm.max - range_norm.min);
        //     const double mean = 0 - range_norm.min;
        //     Normalization(image_to_insert, mean, std);
        // }
        // Ditto
        // if (pre_proc_info->doNeedDistribNormalization()) {
        //     const auto &distrib_norm = pre_proc_info->getDistribNormalization();
        //     Normalization(image_to_insert, distrib_norm.mean, distrib_norm.std);
        // }

        // Set background color
        cv::Scalar background_color;
        if (fill_value.empty()) {
            fill_value.resize(image_to_insert.channels());
            std::fill(fill_value.begin(), fill_value.end(), 0);
        }
        try {
            switch (image_to_insert.channels()) {
            case 4:
                background_color = cv::Scalar(fill_value.at(0), fill_value.at(1), fill_value.at(2), fill_value.at(3));
                break;
            case 3:
                background_color = cv::Scalar(fill_value.at(0), fill_value.at(1), fill_value.at(2));
                break;
            case 2:
                background_color = cv::Scalar(fill_value.at(0), fill_value.at(1));
                break;
            case 1:
                background_color = cv::Scalar(fill_value.at(0));
                break;
            default:
                throw std::runtime_error("Image has unsupported number of channels: " +
                                         std::to_string(image_to_insert.channels()));
            }
        } catch (const std::out_of_range &e) {
            std::throw_with_nested(std::runtime_error("Failed to get values from padding's field \"fill_value\"."));
        }

        int shift_x = (input_size.width - image_to_insert.size().width) / 2;
        int shift_y = (input_size.height - image_to_insert.size().height) / 2;
        cv::Rect region_to_insert(shift_x, shift_y, image_to_insert.size().width, image_to_insert.size().height);

        cv::Mat result(input_size, image_to_insert.type(), background_color);
        cv::Mat region_image_to_insert(result, region_to_insert);
        image_to_insert.copyTo(region_image_to_insert);

        if (image_transform_info)
            image_transform_info->PaddingHasDone(safe_convert<size_t>(shift_x), safe_convert<size_t>(shift_y));

        return result;
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed custom image pre-processing."));
    }
    return cv::Mat();
}

void OpenCV_VPP::Convert(const Image &raw_src, Image &dst, const InputImageLayerDesc::Ptr &pre_proc_info,
                         const ImageTransformationParams::Ptr &image_transform_info, bool make_planar,
                         bool allocate_destination) {
    ITT_TASK("OpenCV_VPP");

    try {
        if (allocate_destination) {
            throw std::invalid_argument("allocate_destination set to true is not supported");
        }

        Image src = ApplyCrop(raw_src);
        // if identical format and resolution
        if (!needPreProcessing(raw_src, dst)) {
            CopyImage(raw_src, dst);
            // do not return here. Code below is mandatory to execute landmarks inference on CentOS in case of vaapi
            // pre-proc
        }

        cv::Mat src_mat_image;
        cv::Mat dst_mat_image;

        auto converted_format = ImageToMat(src, src_mat_image);

        if (needCustomImageConvert(pre_proc_info)) {
            if (dst.height > safe_convert<uint32_t>(std::numeric_limits<int>::max()) and
                dst.width > safe_convert<uint32_t>(std::numeric_limits<int>::max()))
                throw std::runtime_error("Image size too large.");

            cv::Size dst_size(safe_convert<int>(dst.width), safe_convert<int>(dst.height));
            dst_mat_image =
                CustomImageConvert(src_mat_image, converted_format, dst_size, pre_proc_info, image_transform_info);
        } else {
            dst_mat_image = ResizeMat(src_mat_image, dst.height, dst.width);
        }

        if (make_planar) {
            MatToMultiPlaneImage(dst_mat_image, dst);
        } else {
            if (GlobUtils::GetPlanesCount(dst.format) != 1)
                throw std::runtime_error(
                    "Formats with more than one plane could not be processed with `make_planar=false`");
            auto channels_count = GlobUtils::GetChannelsCount(dst.format);
            cv::Mat dst_mat(safe_convert<int>(dst.height), safe_convert<int>(dst.width),
                            CV_MAKE_TYPE(CV_8U, channels_count), dst.planes[0], dst.stride[0]);
            dst_mat_image.copyTo(dst_mat);
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during OpenCV image pre-processing"));
    }
}

void OpenCV_VPP::ReleaseImage(const Image &) {
}
