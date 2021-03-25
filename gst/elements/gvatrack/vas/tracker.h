/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gstgvatrack.h"
#include "itracker.h"
#include "vas/ot.h"

#include <gst/gst.h>
#include <gst/video/video.h>

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace VasWrapper {

class Tracker : public ITracker {
  private:
    std::unique_ptr<vas::ot::ObjectTracker> object_tracker;
    const GstGvaTrack *gva_track;
    std::unordered_map<int, std::string> labels;
    vas::ot::TrackingType tracker_type;
    cv::Mat cv_empty_mat;

  public:
    Tracker(const GstGvaTrack *gva_track, vas::ot::TrackingType tracking_type);
    ~Tracker() = default;

    void track(GstBuffer *buffer) override;
};

} // namespace VasWrapper
