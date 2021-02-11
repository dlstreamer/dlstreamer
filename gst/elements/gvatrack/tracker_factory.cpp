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
    bool result = true;

#if defined(ENABLE_VAS_TRACKER)
#if defined(DOWNLOAD_VAS_TRACKER)
    result &= TrackerFactory::Register(GstGvaTrackingType::SHORT_TERM, VasWrapper::Tracker::CreateShortTerm);
    result &= TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM, VasWrapper::Tracker::CreateZeroTerm);
#endif
    result &= TrackerFactory::Register(GstGvaTrackingType::SHORT_TERM_IMAGELESS,
                                       VasWrapper::Tracker::CreateShortTermImageless);
    result &=
        TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM_IMAGELESS, VasWrapper::Tracker::CreateZeroTermImageless);
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
