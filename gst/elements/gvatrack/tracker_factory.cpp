/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker_factory.h"
#include "config.h"
#include "iou/tracker.h"

#ifdef ENABLE_VAS_TRACKER
#include "vas/tracker.h"
#endif

std::map<GstGvaTrackingType, TrackerFactory::TrackerCreator> TrackerFactory::registred_trackers;

bool TrackerFactory::registered_all = TrackerFactory::RegisterAll();

bool TrackerFactory::RegisterAll() {
    bool result = true;
    result &= TrackerFactory::Register(GstGvaTrackingType::IOU, iou::Tracker::Create);
#ifdef ENABLE_VAS_TRACKER
    result &= TrackerFactory::Register(GstGvaTrackingType::SHORT_TERM, VasWrapper::Tracker::CreateShortTerm);
    result &= TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM, VasWrapper::Tracker::CreateZeroTerm);
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

ITracker *TrackerFactory::Create(const GstGvaTrackingType tracking_type, const GstVideoInfo *video_info) {
    auto tracker_it = registred_trackers.find(tracking_type);
    if (tracker_it != registred_trackers.end())
        return tracker_it->second(video_info);

    return nullptr;
}
