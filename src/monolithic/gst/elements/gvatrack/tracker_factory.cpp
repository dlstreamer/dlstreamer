/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#include "tracker_factory.h"

#include "deep_sort_tracker.h"
#include "tracker.h"

vas::ColorFormat gstVideoFmtToVasColorFmt(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
        return vas::ColorFormat::BGRX;
    case GST_VIDEO_FORMAT_NV12:
        return vas::ColorFormat::NV12;
    case GST_VIDEO_FORMAT_I420:
        return vas::ColorFormat::I420;
    case GST_VIDEO_FORMAT_BGR:
    default:
        return vas::ColorFormat::BGR;
    }
}

std::map<GstGvaTrackingType, TrackerFactory::TrackerCreator> TrackerFactory::registred_trackers;

bool TrackerFactory::registered_all = TrackerFactory::RegisterAll();

bool TrackerFactory::RegisterAll() {
    using namespace std::placeholders;
    using namespace vas::ot;

    bool result = true;

    auto create_vas_tracker = [](const GstGvaTrack *gva_track, TrackingType tracking_type,
                                 dlstreamer::MemoryMapperPtr mapper, dlstreamer::ContextPtr context) -> ITracker * {
        const vas::ColorFormat color_fmt = gstVideoFmtToVasColorFmt(gva_track->info->finfo->format);
        const std::string cfg = gva_track->tracking_config ? gva_track->tracking_config : std::string();
        // cannot use smart pointers here
        return new VasWrapper::Tracker(gva_track->device, tracking_type, color_fmt, cfg, std::move(mapper),
                                       std::move(context));
    };

    result &=
        TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM,
                                 std::bind(create_vas_tracker, _1, TrackingType::ZERO_TERM_COLOR_HISTOGRAM, _2, _3));
    result &= TrackerFactory::Register(GstGvaTrackingType::SHORT_TERM_IMAGELESS,
                                       std::bind(create_vas_tracker, _1, TrackingType::SHORT_TERM_IMAGELESS, _2, _3));
    result &= TrackerFactory::Register(GstGvaTrackingType::ZERO_TERM_IMAGELESS,
                                       std::bind(create_vas_tracker, _1, TrackingType::ZERO_TERM_IMAGELESS, _2, _3));

    // Register Deep SORT tracker - handles both with and without feature model
    auto create_deep_sort_tracker = [](const GstGvaTrack *gva_track, dlstreamer::MemoryMapperPtr mapper,
                                       dlstreamer::ContextPtr /*context*/) -> ITracker * {
        std::string feature_model_path = gva_track->feature_model ? gva_track->feature_model : "";

        if (!feature_model_path.empty()) {
            // Create Deep SORT tracker with feature extraction model
            std::string device = gva_track->device ? gva_track->device : "CPU";
            return new DeepSortWrapper::DeepSortTracker(
                feature_model_path, device, DeepSortWrapper::DEFAULT_MAX_IOU_DISTANCE, DeepSortWrapper::DEFAULT_MAX_AGE,
                DeepSortWrapper::DEFAULT_N_INIT, DeepSortWrapper::DEFAULT_MAX_COSINE_DISTANCE,
                DeepSortWrapper::DEFAULT_NN_BUDGET, std::move(mapper));
        } else {
            // Create Deep SORT tracker without feature extraction model (appearance features disabled)
            return new DeepSortWrapper::DeepSortTracker(
                DeepSortWrapper::DEFAULT_MAX_IOU_DISTANCE, DeepSortWrapper::DEFAULT_MAX_AGE,
                DeepSortWrapper::DEFAULT_N_INIT, DeepSortWrapper::DEFAULT_MAX_COSINE_DISTANCE,
                DeepSortWrapper::DEFAULT_NN_BUDGET, std::move(mapper));
        }
    };
    result &= TrackerFactory::Register(GstGvaTrackingType::DEEP_SORT, create_deep_sort_tracker);

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

ITracker *TrackerFactory::Create(const GstGvaTrack *gva_track, dlstreamer::MemoryMapperPtr mapper,
                                 dlstreamer::ContextPtr context) {
    if (!gva_track)
        throw std::invalid_argument("GvaTrack instance is null");

    auto tracker_it = registred_trackers.find(gva_track->tracking_type);
    if (tracker_it != registred_trackers.end())
        return tracker_it->second(gva_track, std::move(mapper), std::move(context));

    return nullptr;
}
