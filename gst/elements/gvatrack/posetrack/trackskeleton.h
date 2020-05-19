/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "itracker.h"

#include "video_frame.h"
#include <gst/gst.h>
#include <gst/video/video.h>
#include <memory>
#include <mutex>

#include <unordered_map>
namespace skeletontracker {
class Tracker : public ITracker {
  protected:
    std::vector<std::map<std::string, float>> poses;
    int object_id = 0;
    float threshold;
    int frame_width;
    int frame_height;

  public:
    Tracker(int _frame_width, int _frame_height,
            std::vector<std::map<std::string, float>> _poses = std::vector<std::map<std::string, float>>(),
            int _object_id = 0, float _threshold = 0.5f);
    ~Tracker() = default;
    void track(GstBuffer *buffer) override;
    static ITracker *Create(const GstVideoInfo *video_info);
    float Distance(const GVA::Tensor &tensor, const std::map<std::string, float> &pose);
    void copyTensorsToPoses(const std::vector<GVA::Tensor> &tensors, std::vector<std::map<std::string, float>> &poses);
};
} // namespace skeletontracker