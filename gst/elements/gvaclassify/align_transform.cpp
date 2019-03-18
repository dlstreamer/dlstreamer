/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "reid_gallery.h"

#include "inference_backend/image_inference.h"
#include <algorithm>
#include <limits>
#include <opencv2/imgproc.hpp>
#include <vector>

cv::Mat GetTransform(cv::Mat *src, cv::Mat *dst) {
    cv::Mat col_mean_src;
    reduce(*src, col_mean_src, 0, cv::REDUCE_AVG);
    for (int i = 0; i < src->rows; i++) {
        src->row(i) -= col_mean_src;
    }

    cv::Mat col_mean_dst;
    reduce(*dst, col_mean_dst, 0, cv::REDUCE_AVG);
    for (int i = 0; i < dst->rows; i++) {
        dst->row(i) -= col_mean_dst;
    }

    cv::Scalar mean, dev_src, dev_dst;
    cv::meanStdDev(*src, mean, dev_src);
    dev_src(0) = std::max(static_cast<double>(std::numeric_limits<float>::epsilon()), dev_src(0));
    *src /= dev_src(0);
    cv::meanStdDev(*dst, mean, dev_dst);
    dev_dst(0) = std::max(static_cast<double>(std::numeric_limits<float>::epsilon()), dev_dst(0));
    *dst /= dev_dst(0);

    cv::Mat w, u, vt;
    cv::SVD::compute((*src).t() * (*dst), w, u, vt);
    cv::Mat r = (u * vt).t();
    cv::Mat m(2, 3, CV_32F);
    m.colRange(0, 2) = r * (dev_dst(0) / dev_src(0));
    m.col(2) = (col_mean_dst.t() - m.colRange(0, 2) * col_mean_src.t());
    return m;
}

void align_rgb_image(InferenceBackend::Image &image, const std::vector<float> &landmarks_points,
                     const std::vector<float> &reference_points) {
    cv::Mat ref_landmarks = cv::Mat(reference_points.size() / 2, 2, CV_32F);
    cv::Mat landmarks =
        cv::Mat(landmarks_points.size() / 2, 2, CV_32F, const_cast<float *>(&landmarks_points.front())).clone();

    for (int i = 0; i < ref_landmarks.rows; i++) {
        ref_landmarks.at<float>(i, 0) = reference_points[2 * i] * image.width;
        ref_landmarks.at<float>(i, 1) = reference_points[2 * i + 1] * image.height;
        landmarks.at<float>(i, 0) *= image.width;
        landmarks.at<float>(i, 1) *= image.height;
    }
    cv::Mat m = GetTransform(&ref_landmarks, &landmarks);
    for (int plane_num = 0; plane_num < 4; plane_num++) {
        if (image.planes[plane_num]) {
            cv::Mat mat0(image.height, image.width, CV_8UC1, image.planes[plane_num], image.stride[plane_num]);
            cv::warpAffine(mat0, mat0, m, mat0.size(), cv::WARP_INVERSE_MAP);
        }
    }
}
