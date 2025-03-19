/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "object_tracker.h"

#include "dlstreamer/utils.h"

#include "tracker.h"
#include <memory>

namespace vas {
namespace ot {

const float kDefaultDeltaTime = 0.033f;

ObjectTracker::ObjectTracker(const InitParameters &param)
    : delta_t_(kDefaultDeltaTime), tracking_per_class_(param.tracking_per_class) {
    tracker_.reset(vas::ot::Tracker::CreateInstance(param));

    produced_tracklets_.clear();
}

ObjectTracker::~ObjectTracker() {
}

void ObjectTracker::SetDeltaTime(float delta_t) {
    DLS_CHECK(delta_t >= 0.005f && delta_t <= 0.5f);
    delta_t_ = delta_t;
}

std::vector<Object> ObjectTracker::Track(cv::Size frame_size, const std::vector<DetectedObject> &detected_objects) {
    DLS_CHECK(frame_size.width > 0 && frame_size.height > 0);
    int32_t frame_w = frame_size.width;
    int32_t frame_h = frame_size.height;
    cv::Rect frame_rect(0, 0, frame_w, frame_h);

    // TRACE("START");
    std::vector<vas::ot::Detection> detections;

    // TRACE("+ Number: Detected objects (%d)", static_cast<int32_t>(detected_objects.size()));
    int32_t index = 0;
    for (const auto &object : detected_objects) {
        vas::ot::Detection detection;

        detection.class_label = object.class_label;
        detection.feature = object.feature.clone(); // deep copy
        detection.rect = static_cast<cv::Rect2f>(object.rect);
        detection.index = index;

        detections.emplace_back(detection);
        index++;
    }

    std::vector<Object> objects;
    tracker_->TrackObjects(frame_size, detections, &produced_tracklets_, delta_t_);
    // TRACE("+ Number: Tracking objects (%d)", static_cast<int32_t>(produced_tracklets_.size()));

    for (const auto &tracklet : produced_tracklets_) // result 'Tracklet'
    {
        cv::Rect rect = static_cast<cv::Rect>(tracklet->trajectory_filtered.back());
        if ((rect & frame_rect).area() > 0) {
            Object object;
            // TRACE("     - ID(%d) Status(%d)", tracklet.id, tracklet.status);
            object.rect = static_cast<cv::Rect>(tracklet->trajectory_filtered.back());
            object.tracking_id = tracklet->id;
            object.class_label = tracklet->label;
            object.association_idx = tracklet->association_idx;
            object.status = vas::ot::TrackingStatus::LOST;
            switch (tracklet->status) {
            case ST_NEW:
                object.status = vas::ot::TrackingStatus::NEW;
                break;
            case ST_TRACKED:
                object.status = vas::ot::TrackingStatus::TRACKED;
                break;
            case ST_LOST:
            default:
                object.status = vas::ot::TrackingStatus::LOST;
            }
            objects.emplace_back(object);
        } else {
            // TRACE("[ %d, %d, %d, %d ] is out of image bound! -> removed", rect.x, rect.y, rect.width, rect.height);
        }
    }
    // TRACE("+ Number: Result objects (%d)", static_cast<int32_t>(objects.size()));

    // TRACE("END");
    return objects;
}

}; // namespace ot
}; // namespace vas
