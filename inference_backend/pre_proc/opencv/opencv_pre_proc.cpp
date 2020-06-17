/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
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

PreProc *InferenceBackend::CreatePreProcOpenCV() {
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

void OpenCV_VPP::Convert(const Image &raw_src, Image &dst, bool bAllocateDestination) {
    ITT_TASK("OpenCV_VPP");

    try {
        if (bAllocateDestination) {
            throw std::invalid_argument("bAllocateDestination set to true is not supported");
        }

        Image src = ApplyCrop(raw_src);
        // if identical format and resolution
        if (src.format == dst.format and
            (src.format == FourCC::FOURCC_RGBP or src.format == FourCC::FOURCC_RGBP_F32) and src.width == dst.width and
            src.height == dst.height) {
            CopyImage(raw_src, dst);
        }

        cv::Mat mat_image;
        ImageToMat(src, mat_image);
        cv::Mat resized_image = ResizeMat(mat_image, dst.height, dst.width);
        MatToMultiPlaneImage(resized_image, dst);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed during OpenCV image pre-processing"));
    }
}

void OpenCV_VPP::ReleaseImage(const Image &) {
}
