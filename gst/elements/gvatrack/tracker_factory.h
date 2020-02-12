/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "itracker.h"
#include "tracker_types.h"

#include <functional>
#include <gst/video/video.h>
#include <map>
#include <string>

class TrackerFactory {
  public:
    using TrackerCreator = std::function<ITracker *(const GstVideoInfo *video_info)>;
    TrackerFactory() = delete;
    static bool Register(const GstGvaTrackingType, TrackerCreator);
    static ITracker *Create(const GstGvaTrackingType tracking_type, const GstVideoInfo *video_info);

  private:
    static bool RegisterAll();

    static bool registered_all;
    static std::map<GstGvaTrackingType, TrackerCreator> registred_trackers;
};
