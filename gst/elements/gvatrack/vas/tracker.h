/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "itracker.h"
#include "loader.h"
#include "vas/ot.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <memory>
#include <mutex>

#include <unordered_map>
namespace VasWrapper {

class Tracker : public ITracker {
  private:
    Loader loader;
    std::unique_ptr<vas::ot::ObjectTracker> object_tracker;
    std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>> video_info;
    std::unordered_map<int, std::string> labels;

  public:
    Tracker(const GstVideoInfo *video_info, const std::string &tracking_type);
    ~Tracker() = default;

    static ITracker *CreateShortTerm(const GstVideoInfo *video_info);
    static ITracker *CreateZeroTerm(const GstVideoInfo *video_info);

    void track(GstBuffer *buffer) override;
};

} // namespace VasWrapper
