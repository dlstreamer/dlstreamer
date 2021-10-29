/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
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
    vas::BackendType backend_type;
    vas::ot::TrackingType tracker_type;
    cv::Mat cv_empty_mat;
    std::unique_ptr<class BufferMapper> buffer_mapper;

    struct VaApiEntity;
    std::unique_ptr<VaApiEntity> vaapi;

    void buildGPUTracker(vas::ot::ObjectTracker::Builder *builder);

    std::vector<vas::ot::Object> trackCPU(GstBuffer *buffer,
                                          const std::vector<vas::ot::DetectedObject> &detected_objects);

    std::vector<vas::ot::Object> trackGPU(GstBuffer *buffer,
                                          const std::vector<vas::ot::DetectedObject> &detected_objects);

  public:
    Tracker(const GstGvaTrack *gva_track, vas::ot::TrackingType tracking_type);
    ~Tracker() = default;

    void track(GstBuffer *buffer) override;
};

} // namespace VasWrapper
