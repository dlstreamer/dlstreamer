/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_TRACKLET_H__
#define __OT_TRACKLET_H__

#include "dlstreamer/utils.h"
#include <cstdint>
#include <deque>
#include <opencv2/opencv.hpp>

#include "kalman_filter_no_opencv.h"

namespace vas {
namespace ot {

const int32_t kNoMatchDetection = -1;

enum Status {
    ST_DEAD = -1,   // dead
    ST_NEW = 0,     // new
    ST_TRACKED = 1, // tracked
    ST_LOST = 2     // lost but still alive (in the detection phase if it configured)
};

struct Detection {
    cv::Rect2f rect;
    cv::Mat feature;
    int32_t class_label = -1;
    int32_t index = -1;
};

class Tracklet {
  public:
    Tracklet();
    virtual ~Tracklet();

    void ClearTrajectory();
    void InitTrajectory(const cv::Rect2f &bounding_box);
    void AddUpdatedTrajectory(const cv::Rect2f &bounding_box, const cv::Rect2f &corrected_box);
    void UpdateLatestTrajectory(const cv::Rect2f &bounding_box, const cv::Rect2f &corrected_box);
    virtual void RenewTrajectory(const cv::Rect2f &bounding_box);
    virtual std::deque<cv::Mat> *GetRgbFeatures();

  public:
    int32_t id; // If hasnot been assigned : -1 to 0
    int32_t label;
    int32_t association_idx;
    Status status;
    int32_t age;
    float confidence;

    float occlusion_ratio;
    float association_delta_t;
    int32_t association_fail_count;

    std::deque<cv::Rect2f> trajectory;
    std::deque<cv::Rect2f> trajectory_filtered;

    int32_t birth_count;
    std::deque<cv::Mat> rgb_features;
    std::unique_ptr<KalmanFilterNoOpencv> kalman_filter;
};

using ZeroTermChistTracklet = Tracklet;
using ZeroTermImagelessTracklet = Tracklet;
using ShortTermImagelessTracklet = Tracklet;

}; // namespace ot
}; // namespace vas

#endif // __OT_TRACKLET_H__
