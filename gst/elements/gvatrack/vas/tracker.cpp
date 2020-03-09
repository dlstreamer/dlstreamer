/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker.h"

#include "gva_utils.h"
#include "video_frame.h"

#include <functional>

using namespace VasWrapper;

namespace {

constexpr char K_VAS_OT_PATH[] = "libvasot.so";
const int DEFAULT_MAX_NUM_OBJECTS = -1;

using BuilderCreatorFunctionType = decltype(&CreateObjectTrackerBuilder);
using BuilderDeleterFunctionType = decltype(&DeleteObjectTrackerBuilder);

inline bool CaseInsCharCompareN(char a, char b) {
    return (toupper(a) == toupper(b));
}

inline bool CaseInsCompare(const std::string &s1, const std::string &s2) {
    return ((s1.size() == s2.size()) && std::equal(s1.begin(), s1.end(), s2.begin(), CaseInsCharCompareN));
}

vas::ot::TrackingType trackingType(const std::string &tracking_type) {
    if (CaseInsCompare(tracking_type, "ZERO_TERM")) {
        return vas::ot::TrackingType::ZERO_TERM;
    } else if (CaseInsCompare(tracking_type, "SHORT_TERM")) {
        return vas::ot::TrackingType::SHORT_TERM;
    } else {
        std::string message =
            std::string("Unknown tracking name ") + tracking_type.c_str() + ". Will be treated as SHORT_TERM";
        GVA_WARNING(message.c_str());
        return vas::ot::TrackingType::SHORT_TERM;
    }
}

vas::ColorFormat ConvertFormat(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGR:
        return vas::ColorFormat::BGR;
    case GST_VIDEO_FORMAT_BGRx:
        return vas::ColorFormat::BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        return vas::ColorFormat::BGRX;
    case GST_VIDEO_FORMAT_NV12:
        return vas::ColorFormat::NV12;
    default:
        return vas::ColorFormat::BGR;
    }
}

std::vector<vas::ot::DetectedObject> extractDetectedObjects(GVA::VideoFrame &video_frame,
                                                            std::unordered_map<int, std::string> &labels) {
    std::vector<GVA::RegionOfInterest> regions = video_frame.regions();
    std::vector<vas::ot::DetectedObject> detected_objects;
    for (auto it = regions.rbegin(); it != regions.rend(); ++it) {
        int label_id = it->detection().has_field("label_id") ? it->label_id() : std::numeric_limits<int>::max();
        if (labels.find(label_id) == labels.end())
            labels[label_id] = g_quark_to_string(it->meta()->roi_type);
        cv::Rect obj_rect(it->meta()->x, it->meta()->y, it->meta()->w, it->meta()->h);
        detected_objects.emplace_back(obj_rect, label_id);
        video_frame.pop_region();
    }
    return detected_objects;
}

void append(GVA::VideoFrame &video_frame, const vas::ot::Object &tracked_object, const std::string &label) {
    auto roi = video_frame.add_region(tracked_object.rect.x, tracked_object.rect.y, tracked_object.rect.width,
                                      tracked_object.rect.height, tracked_object.class_label);
    const int tracking_id = tracked_object.tracking_id + 1; // gvaclassify identify tracking id starts 1.
    roi.meta()->roi_type = g_quark_from_string(label.c_str());
    set_object_id(roi.meta(), tracking_id);
}

} // namespace

Tracker::Tracker(const GstVideoInfo *video_info, const std::string &tracking_type)
    : loader(K_VAS_OT_PATH), video_info(gst_video_info_copy(video_info), gst_video_info_free) {
    if (tracking_type.empty() || video_info == nullptr) {
        throw std::invalid_argument("Tracker::Tracker: nullptr arguments is not allowed");
    }
    auto builder_factory_function = loader.load<BuilderCreatorFunctionType>("CreateObjectTrackerBuilder");
    auto builder_deleter_function = loader.load<BuilderDeleterFunctionType>("DeleteObjectTrackerBuilder");

    auto builder = std::unique_ptr<vas::ot::ObjectTracker::Builder, BuilderDeleterFunctionType>(
        builder_factory_function(), builder_deleter_function);
    builder->input_image_format = ConvertFormat(video_info->finfo->format);
    builder->max_num_objects = DEFAULT_MAX_NUM_OBJECTS;

    object_tracker = builder->Build(trackingType(tracking_type));
}

void Tracker::track(GstBuffer *buffer) {
    if (buffer == nullptr)
        throw std::invalid_argument("buffer is nullptr");
    try {
        GVA::VideoFrame video_frame(buffer, video_info.get());
        std::vector<vas::ot::DetectedObject> detected_objects = extractDetectedObjects(video_frame, labels);

        const auto tracked_objects = object_tracker->Track(video_frame.data()->mat(), detected_objects);

        for (const auto &tracked_object : tracked_objects) {
            if (tracked_object.status == vas::ot::TrackingStatus::LOST)
                continue;
            auto it = labels.find(tracked_object.class_label);
            std::string label = it != labels.end() ? it->second : std::string();
            append(video_frame, tracked_object, label);
        }
    } catch (const std::exception &e) {
        GST_ERROR("Exception within tracker occured: %s", e.what());
        throw std::runtime_error("Track: error while tracking objects");
    }
}

ITracker *Tracker::CreateShortTerm(const GstVideoInfo *video_info) {
    return new Tracker(video_info, "SHORT_TERM");
}

ITracker *Tracker::CreateZeroTerm(const GstVideoInfo *video_info) {
    return new Tracker(video_info, "ZERO_TERM");
}
