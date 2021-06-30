/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gstgvatrack.h"
#include "itracker.h"
#include "vas/ot.h"

#ifdef ENABLE_VAAPI
#include "vaapi_converter.h"
#endif

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
    std::unique_ptr<vas::ot::ObjectTracker::Builder> builder;
#ifdef ENABLE_VAAPI
    std::unique_ptr<InferenceBackend::VaApiContext> vaapi_context;
    std::unique_ptr<InferenceBackend::VaApiConverter> vaapi_converter;
    std::vector<vas::ot::Object> trackGPU(GstBuffer *buffer,
                                          const std::vector<vas::ot::DetectedObject> &detected_objects);
#endif

  public:
    Tracker(const GstGvaTrack *gva_track, vas::ot::TrackingType tracking_type);
    ~Tracker() = default;

    void track(GstBuffer *buffer) override;
};

} // namespace VasWrapper
