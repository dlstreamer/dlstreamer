/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gstgvatrack.h"
#include "itracker.h"
#include "tracker_types.h"
#include <dlstreamer/base/memory_mapper.h>
#include <dlstreamer/context.h>

#include <functional>
#include <map>

class TrackerFactory {
  public:
    using TrackerCreator = std::function<ITracker *(const GstGvaTrack *gva_track, dlstreamer::MemoryMapperPtr mapper,
                                                    dlstreamer::ContextPtr context)>;
    TrackerFactory() = delete;
    static bool Register(const GstGvaTrackingType, TrackerCreator);
    static ITracker *Create(const GstGvaTrack *gva_track, dlstreamer::MemoryMapperPtr mapper,
                            dlstreamer::ContextPtr context);

  private:
    static bool RegisterAll();

    static bool registered_all;
    static std::map<GstGvaTrackingType, TrackerCreator> registred_trackers;
};
