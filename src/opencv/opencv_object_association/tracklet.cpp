/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracklet.h"
#include <sstream>

namespace vas {
namespace ot {

Tracklet::Tracklet()
    : id(0), label(-1), association_idx(kNoMatchDetection), status(ST_DEAD), age(0), confidence(0.f),
      occlusion_ratio(0.f), association_delta_t(0.f), association_fail_count(0), birth_count(1) {
}

Tracklet::~Tracklet() {
}

void Tracklet::ClearTrajectory() {
    trajectory.clear();
    trajectory_filtered.clear();
}

void Tracklet::InitTrajectory(const cv::Rect2f &bounding_box) {
    trajectory.push_back(bounding_box);
    trajectory_filtered.push_back(bounding_box);
}

void Tracklet::AddUpdatedTrajectory(const cv::Rect2f &bounding_box, const cv::Rect2f &corrected_box) {
    trajectory.push_back(bounding_box);
    trajectory_filtered.push_back(corrected_box);
}

void Tracklet::UpdateLatestTrajectory(const cv::Rect2f &bounding_box, const cv::Rect2f &corrected_box) {
    trajectory.back() = bounding_box;
    trajectory_filtered.back() = corrected_box;
}

std::deque<cv::Mat> *Tracklet::GetRgbFeatures() {
    return &rgb_features;
}

void Tracklet::RenewTrajectory(const cv::Rect2f &bounding_box) {
    float velo_x = bounding_box.x - trajectory.back().x;
    float velo_y = bounding_box.y - trajectory.back().y;
    cv::Rect rect_predict(bounding_box.x + velo_x / 3, bounding_box.y + velo_y / 3, bounding_box.width,
                          bounding_box.height);

    ClearTrajectory();
    kalman_filter.reset(new KalmanFilterNoOpencv(bounding_box));
    kalman_filter->Predict();
    kalman_filter->Correct(rect_predict);

    trajectory.push_back(bounding_box);
    trajectory_filtered.push_back(bounding_box);
}

}; // namespace ot
}; // namespace vas
