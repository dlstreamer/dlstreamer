/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gstgvatrack.h"
#include "itracker.h"
#include "tracker_types.h"

#include <functional>
#include <gst/video/video.h>
#include <map>
#include <string>

class TrackerFactory {
  public:
    using TrackerCreator = std::function<ITracker *(const GstGvaTrack *gva_track)>;
    TrackerFactory() = delete;
    static bool Register(const GstGvaTrackingType, TrackerCreator);
    static ITracker *Create(const GstGvaTrack *gva_track);

  private:
    static bool RegisterAll();

    static bool registered_all;
    static std::map<GstGvaTrackingType, TrackerCreator> registred_trackers;
};
