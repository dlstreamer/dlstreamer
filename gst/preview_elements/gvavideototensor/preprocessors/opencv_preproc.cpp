/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "opencv_preproc.hpp"

#include <opencv_utils/opencv_utils.h>

#include <frame_data.hpp>

using namespace InferenceBackend;

namespace {
int get_planes_num_from_preproc(const InputImageLayerDesc::Ptr &pre_proc_info) {
    if (!pre_proc_info)
        return 0;

    switch (pre_proc_info->getTargetColorSpace()) {
    case InputImageLayerDesc::ColorSpace::YUV:
        // TODO: YUV is not implemented yet. Refactor later for supported YUV
        throw std::runtime_error("Unsupported YUV color space format");
    case InputImageLayerDesc::ColorSpace::BGR:
    case InputImageLayerDesc::ColorSpace::RGB:
        return 3;
    case InputImageLayerDesc::ColorSpace::GRAYSCALE:
        return 1;
    case InputImageLayerDesc::ColorSpace::NO:
    default:
        return 0;
    }
}

cv::Mat preprocess_mat(const cv::Mat &src_mat, const int src_color_format, const cv::Size &dst_size,
                       const InputImageLayerDesc::Ptr &pre_proc_info) {
    if (!pre_proc_info)
        return src_mat;

    cv::Mat result_img(src_mat.size(), src_mat.type());

    if (pre_proc_info->doNeedColorSpaceConversion(src_color_format)) {
        Utils::ColorSpaceConvert(src_mat, result_img, src_color_format, pre_proc_info->getTargetColorSpace());
    } else {
        result_img = src_mat;
    }

    if (pre_proc_info->doNeedResize() && result_img.size() != dst_size) {
        switch (pre_proc_info->getResizeType()) {
        case InputImageLayerDesc::Resize::NO_ASPECT_RATIO:
            Utils::Resize(result_img, dst_size);
            break;
        case InputImageLayerDesc::Resize::ASPECT_RATIO: {
            // TODO: think about aspect ration resize + crop more carefully
            auto scale_param = pre_proc_info->doNeedCrop() ? 0 : 8;
            Utils::ResizeAspectRatio(result_img, dst_size, nullptr, scale_param, !pre_proc_info->doNeedCrop());
            break;
        }
        default:
            break;
        }
    }

    if (pre_proc_info->doNeedCrop() && result_img.size() != dst_size) {
        cv::Point2i tl_point;
        auto src_size = result_img.size();
        switch (pre_proc_info->getCropType()) {
        case InputImageLayerDesc::Crop::CENTRAL:
            tl_point = {(src_size.width - dst_size.width) / 2, (src_size.height - dst_size.height) / 2};
            break;
        case InputImageLayerDesc::Crop::TOP_LEFT:
            tl_point = {0, 0};
            break;
        case InputImageLayerDesc::Crop::TOP_RIGHT:
            tl_point = {src_size.width - dst_size.width, 0};
            break;
        case InputImageLayerDesc::Crop::BOTTOM_LEFT:
            tl_point = {0, src_size.height - dst_size.height};
            break;
        case InputImageLayerDesc::Crop::BOTTOM_RIGHT:
            tl_point = {src_size.width - dst_size.width, src_size.height - dst_size.height};
            break;
        default:
            break;
        }
        Utils::Crop(result_img, {tl_point, dst_size}, nullptr);
    }

    return result_img;
}
} // namespace

OpenCVPreProc::OpenCVPreProc(GstVideoInfo *input_video_info, const TensorCaps &output_tensor_info,
                             const InputImageLayerDesc::Ptr &pre_proc_info)
    : _input_video_info(input_video_info), _output_tensor_info(output_tensor_info), _pre_proc_info(pre_proc_info) {
}

void OpenCVPreProc::process(GstBuffer *in_buffer, GstBuffer *out_buffer) {
    FrameData src;
    src.Map(in_buffer, _input_video_info, InferenceBackend::MemoryType::SYSTEM, GST_MAP_READ);

    FrameData dst;
    dst.Map(out_buffer, _output_tensor_info, GST_MAP_WRITE, get_planes_num_from_preproc(_pre_proc_info));

    cv::Mat src_mat;
    uint8_t *src_planes[MAX_PLANES_NUM];
    uint32_t src_strides[MAX_PLANES_NUM];
    uint32_t src_offsets[MAX_PLANES_NUM];

    // TODO: use std::vector instead of raw array pointers
    for (guint i = 0; i < src.GetPlanesNum(); i++) {
        src_planes[i] = src.GetPlane(i);
        src_strides[i] = src.GetStride(i);
        src_offsets[i] = src.GetOffset(i);
    }

    Utils::CreateMat(src_planes, src.GetWidth(), src.GetHeight(), src.GetFormat(), src_strides, src_offsets, src_mat);

    auto res = preprocess_mat(src_mat, src.GetFormat(), cv::Size(dst.GetWidth(), dst.GetHeight()), _pre_proc_info);

    uint8_t *dst_planes[MAX_PLANES_NUM];
    for (guint i = 0; i < dst.GetPlanesNum(); i++)
        dst_planes[i] = dst.GetPlane(i);
    Utils::MatToMultiPlaneImage(res, dst.GetFormat(), dst.GetWidth(), dst.GetHeight(), dst_planes);
}

void OpenCVPreProc::process(GstBuffer *) {
    throw std::runtime_error("OpenCVPreProc: In-place processing is not supported");
}
