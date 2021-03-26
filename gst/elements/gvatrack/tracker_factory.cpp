/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_factory.h"
#include "config.h"

#ifdef ENABLE_VAS_TRACKER
#include "vas/tracker.h"
#endif

std::map<GstGvaTrackingType, TrackerFactory::TrackerCreator> TrackerFactory::registred_trackers;

bool TrackerFactory::registered_all = TrackerFactory::RegisterAll();

bool TrackerFactory::RegisterAll() {
    using namespace std::placeholders;
    using namespace vas::ot;

    bool result = true;

#ifdef ENABLE_VAS_TRACKER
    auto create_vas_tracker = [](const GstGvaTrack *gva_track, TrackingType tracking_type) -> ITracker * {
        return new VasWrapper::Tracker(gva_track, tracking_type);
    };

    result &= TrackerFactory::Register(GstGvaTrackingType::SHORT_TERM,
                                       std::bind(create_vas_tracker, _1, TrackingType::SHORT_TERM_KCFVAR));
    result &= TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM,
                                       std::bind(create_vas_tracker, _1, TrackingType::ZERO_TERM_COLOR_HISTOGRAM));
    result &= TrackerFactory::Register(GstGvaTrackingType::SHORT_TERM_IMAGELESS,
                                       std::bind(create_vas_tracker, _1, TrackingType::SHORT_TERM_IMAGELESS));
    result &= TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM_IMAGELESS,
                                       std::bind(create_vas_tracker, _1, TrackingType::ZERO_TERM_IMAGELESS));
#endif

    return result;
}

bool TrackerFactory::Register(const GstGvaTrackingType tracking_type, TrackerCreator func_create) {
    auto tracker_it = registred_trackers.find(tracking_type);
    if (tracker_it == registred_trackers.end()) {
        registred_trackers[tracking_type] = func_create;
        return true;
    }

    return false;
}

ITracker *TrackerFactory::Create(const GstGvaTrack *gva_track) {
    auto tracker_it = registred_trackers.find(gva_track->tracking_type);
    if (tracker_it != registred_trackers.end())
        return tracker_it->second(gva_track);

    return nullptr;
}
