/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
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

void OpenCV_VPP::Convert(const Image &rawSrc, Image &dst, bool bAllocateDestination) {
    ITT_TASK("OpenCV_VPP");

    if (bAllocateDestination) {
        throw std::runtime_error("OpenCV_VPP: bAllocateDestination==true not supported");
    }

    Image src = ApplyCrop(rawSrc);
    // if identical format and resolution
    if (src.format == dst.format && src.format == FourCC::FOURCC_RGBP && src.width == dst.width &&
        src.height == dst.height) {
        int planes_count = GetPlanesCount(src.format);
        for (int i = 0; i < planes_count; i++) {
            if (src.width == src.stride[i]) {
                memcpy(dst.planes[i], src.planes[i], src.width * src.height * sizeof(uint8_t));
            } else {
                int dst_stride = src.width * sizeof(uint8_t);
                int src_stride = src.stride[i] * sizeof(uint8_t);
                for (int r = 0; r < src.height; r++) {
                    memcpy(dst.planes[i] + r * dst_stride, src.planes[i] + r * src_stride, dst_stride);
                }
            }
        }
    }

    cv::Mat mat_image;
    ImageToMat(src, mat_image);
    cv::imwrite("/home/pbochenk/projects/diplom/crop/crop.png", mat_image);
    cv::Mat resized_image = ResizeMat(mat_image, dst.height, dst.width);
    MatToMultiPlaneImage(resized_image, dst);
}

void OpenCV_VPP::ReleaseImage(const Image &) {
}
