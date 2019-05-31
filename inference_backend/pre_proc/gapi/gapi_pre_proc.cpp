/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "gapi_pre_proc.h"

#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"
#include "opencv_utils.h"

#include <gst/gst.h>

#include <opencv2/gapi.hpp>
#include <opencv2/gapi/core.hpp>
#include <opencv2/gapi/imgproc.hpp>

using namespace InferenceBackend;
using namespace InferenceBackend::Utils;

PreProc *InferenceBackend::CreatePreProcGAPI() {
    return new GAPI_VPP();
}

GAPI_VPP::GAPI_VPP() {
    Type = MemoryType::SYSTEM;
}

GAPI_VPP::~GAPI_VPP() {
}

void GAPI_VPP::Convert(const Image &src, Image &dst, bool bAllocateDestination) {
    if (bAllocateDestination) {
        throw std::runtime_error("GAPI_VPP: bAllocateDestination==true not supported");
    }

    cv::GMat y_mat, uv_mat; // for NV12 only
    cv::GMat bgr_mat_to_convert;
    if (src.format == FOURCC_NV12) {
        // Describe 2 NV12 cv::Mat planes to BGR cv::Mat plane conversion
        bgr_mat_to_convert = cv::gapi::NV12toBGR(y_mat, uv_mat);
    }

    cv::GMat cropped_bgr_mat;
    int rect_width = 0;
    int rect_height = 0;
    if (src.rect.width or src.rect.height) {
        // Will be cropped
        if (src.rect.x >= src.width || src.rect.y >= src.height || src.rect.x + src.rect.width <= 0 ||
            src.rect.y + src.rect.height <= 0) {
            throw std::runtime_error("ERROR: Requested rectangle is out of image boundaries\n");
        }

        int rect_x = std::max(src.rect.x, 0);
        int rect_y = std::max(src.rect.y, 0);
        rect_width = std::min(src.rect.width - (rect_x - src.rect.x), src.width - rect_x);
        rect_height = std::min(src.rect.height - (rect_y - src.rect.y), src.height - rect_y);

        cv::Rect crop_rect(rect_x, rect_y, rect_width, rect_height);
        // Describe crop
        cropped_bgr_mat = cv::gapi::crop(bgr_mat_to_convert, crop_rect);
    } else {
        // We do not crop, step "bgr_mat_to_convert -> {crop} -> cropped_bgr_mat" is intentionally missing
        cropped_bgr_mat = bgr_mat_to_convert;
    }

    // Describe resize
    cv::GMat resized_cropped_bgr_mat = cv::gapi::resize(cropped_bgr_mat, cv::Size(dst.width, dst.height), 0, 0);

    // Describe color space conversion BGR -> RGBP
    cv::GMat B, G, R;
    std::tie(B, G, R) = cv::gapi::split3(resized_cropped_bgr_mat);

    // Planes will be updated in-place
    std::vector<cv::Mat> outs{cv::Mat(dst.height, dst.width, CV_8UC1, dst.planes[0], dst.stride[0]),
                              cv::Mat(dst.height, dst.width, CV_8UC1, dst.planes[1], dst.stride[1]),
                              cv::Mat(dst.height, dst.width, CV_8UC1, dst.planes[2], dst.stride[2])};

    if (src.format == FOURCC_NV12) {
        cv::Mat y, uv;
        NV12ImageToMats(src, y, uv);

        // Add described above to graph
        cv::GComputation ac(cv::GIn(y_mat, uv_mat), cv::GOut(B, G, R));
        // Apply graph computation
        ac.apply(cv::gin(y, uv), cv::gout(outs[0], outs[1], outs[2]));
    } else {
        cv::Mat mat_image;
        ImageToMat(src, mat_image);

        // Add described above to graph
        cv::GComputation ac(cv::GIn(bgr_mat_to_convert), cv::GOut(B, G, R));
        // Apply graph computation
        ac.apply(cv::gin(mat_image), cv::gout(outs[0], outs[1], outs[2]));
    }
}

void GAPI_VPP::ReleaseImage(const Image &dst) {
    (void)(dst);
}
