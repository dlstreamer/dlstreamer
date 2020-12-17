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
    Tracker(const GstGvaTrack *gva_track, const std::string &tracking_type);
    ~Tracker() = default;

    static ITracker *CreateShortTerm(const GstGvaTrack *gva_track);
    static ITracker *CreateZeroTerm(const GstGvaTrack *gva_track);
    static ITracker *CreateShortTermImageless(const GstGvaTrack *gva_track);
    static ITracker *CreateZeroTermImageless(const GstGvaTrack *gva_track);

    void track(GstBuffer *buffer) override;
};

} // namespace VasWrapper
