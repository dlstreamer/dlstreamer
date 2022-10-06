/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ot.h"

#include "common.h"

#include "tracker.h"
#include <memory>

namespace vas {
namespace ot {

const float kDefaultDeltaTime = 0.033f;

// Internal implementation: includes OT component
class ObjectTracker::Impl {
  public:
    class InitParameters : public vas::ot::Tracker::InitParameters {
      public:
        TrackingType tracking_type;
    };

  public:
    explicit Impl(const InitParameters &param);

    Impl() = delete;
    ~Impl();
    Impl(const Impl &) = delete;
    Impl(Impl &&) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

  public:
    int32_t GetMaxNumObjects() const noexcept;
    TrackingType GetTrackingType() const noexcept;
    float GetDeltaTime() const noexcept;
    bool GetTrackingPerClass() const noexcept;
    void SetDeltaTime(float delta_t);
    std::vector<Object> Track(cv::Size frame_size, const std::vector<DetectedObject> &objects);

  private:
    std::unique_ptr<vas::ot::Tracker> tracker_;
    std::vector<std::shared_ptr<Tracklet>> produced_tracklets_;

    int32_t max_num_objects_;
    float delta_t_;
    TrackingType tracking_type_;
    bool tracking_per_class_;

    friend class ObjectTracker::Builder;
};

void vas_exit() {
}

ObjectTracker::ObjectTracker(ObjectTracker::Impl *impl) : impl_(impl) {
    atexit(vas_exit);
}

ObjectTracker::~ObjectTracker() = default;

int32_t ObjectTracker::GetMaxNumObjects() const noexcept {
    return impl_->GetMaxNumObjects();
}

TrackingType ObjectTracker::GetTrackingType() const noexcept {
    return impl_->GetTrackingType();
}

float ObjectTracker::GetFrameDeltaTime() const noexcept {
    return impl_->GetDeltaTime();
}

bool ObjectTracker::GetTrackingPerClass() const noexcept {
    return impl_->GetTrackingPerClass();
}

void ObjectTracker::SetFrameDeltaTime(float frame_delta_t) {
    impl_->SetDeltaTime(frame_delta_t);
}

std::vector<Object> ObjectTracker::Track(cv::Size frame_size, const std::vector<DetectedObject> &objects) {
    return impl_->Track(frame_size, objects);
}

ObjectTracker::Impl::Impl(const InitParameters &param)
    : max_num_objects_(param.max_num_objects), delta_t_(kDefaultDeltaTime), tracking_type_(param.tracking_type),
      tracking_per_class_(param.tracking_per_class) {
    TRACE("BEGIN");
    if ((param.max_num_objects) != -1 && (param.max_num_objects <= 0)) {
        printf("Error: Invalid maximum number of objects:  %d\n", param.max_num_objects);
        ETHROW(false, invalid_argument, "Invalid maximum number of objects");
    }

    TRACE("tracking_type: %d, backend_type: %d, color_format: %d, max_num_object: %d, tracking_per_class: %d",
          static_cast<int32_t>(tracking_type_), static_cast<int32_t>(backend_type_),
          static_cast<int32_t>(input_color_format_), max_num_objects_, tracking_per_class_);

    tracker_.reset(vas::ot::Tracker::CreateInstance(param));

    produced_tracklets_.clear();

    TRACE("END");
}

ObjectTracker::Impl::~Impl() {
}

void ObjectTracker::Impl::SetDeltaTime(float delta_t) {
    if (delta_t < 0.005f || delta_t > 0.5f) {
        printf("Error: Invalid argument for SetFrameDeltaTime %f\n", delta_t);
        ETHROW(false, invalid_argument, "Invalid argument for SetFrameDeltaTime");
    }

    delta_t_ = delta_t;
    return;
}

int32_t ObjectTracker::Impl::GetMaxNumObjects() const noexcept {
    return max_num_objects_;
}

TrackingType ObjectTracker::Impl::GetTrackingType() const noexcept {
    return tracking_type_;
}

float ObjectTracker::Impl::GetDeltaTime() const noexcept {
    return delta_t_;
}

bool ObjectTracker::Impl::GetTrackingPerClass() const noexcept {
    return tracking_per_class_;
}

std::vector<Object> ObjectTracker::Impl::Track(cv::Size frame_size,
                                               const std::vector<DetectedObject> &detected_objects) {
    if (frame_size.width <= 0 || frame_size.height <= 0) {
        printf("Error: Invalid frame size");
        ETHROW(false, invalid_argument, "Invalid frame size");
    }
    int32_t frame_w = frame_size.width;
    int32_t frame_h = frame_size.height;
    cv::Rect frame_rect(0, 0, frame_w, frame_h);

    TRACE("START");
    std::vector<vas::ot::Detection> detections;

    TRACE("+ Number: Detected objects (%d)", static_cast<int32_t>(detected_objects.size()));
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
    TRACE("+ Number: Tracking objects (%d)", static_cast<int32_t>(produced_tracklets_.size()));

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
            TRACE("[ %d, %d, %d, %d ] is out of the image bound! -> Filtered out.", rect.x, rect.y, rect.width,
                  rect.height);
        }
    }
    TRACE("+ Number: Result objects (%d)", static_cast<int32_t>(objects.size()));

    TRACE("END");
    return objects;
}

ObjectTracker::Builder::Builder()
    : max_num_objects(kDefaultMaxNumObjects), tracking_per_class(true), kRgbHistDistScale(0.25f),
      kNormCenterDistScale(0.5f), kNormShapeDistScale(0.75f) {
}

ObjectTracker::Builder::~Builder() {
}

std::unique_ptr<ObjectTracker> ObjectTracker::Builder::Build(TrackingType tracking_type) const {
    TRACE("BEGIN");

    ObjectTracker::Impl *ot_impl = nullptr;
    ObjectTracker::Impl::InitParameters param;

    param.max_num_objects = max_num_objects;
    param.tracking_type = tracking_type;
    param.tracking_per_class = tracking_per_class;
    param.kRgbHistDistScale = kRgbHistDistScale;
    param.kNormCenterDistScale = kNormCenterDistScale;
    param.kNormShapeDistScale = kNormShapeDistScale;

    switch (tracking_type) {
    case vas::ot::TrackingType::LONG_TERM:
        param.profile = vas::ot::Tracker::PROFILE_LONG_TERM;
        break;
    case vas::ot::TrackingType::SHORT_TERM:
        param.profile = vas::ot::Tracker::PROFILE_SHORT_TERM;
        break;
    case vas::ot::TrackingType::SHORT_TERM_KCFVAR:
        param.profile = vas::ot::Tracker::PROFILE_SHORT_TERM_KCFVAR;
        break;
    case vas::ot::TrackingType::SHORT_TERM_IMAGELESS:
        param.profile = vas::ot::Tracker::PROFILE_SHORT_TERM_IMAGELESS;
        break;
    case vas::ot::TrackingType::ZERO_TERM:
        param.profile = vas::ot::Tracker::PROFILE_ZERO_TERM;
        break;
    case vas::ot::TrackingType::ZERO_TERM_COLOR_HISTOGRAM:
        param.profile = vas::ot::Tracker::PROFILE_ZERO_TERM_COLOR_HISTOGRAM;
        break;
    case vas::ot::TrackingType::ZERO_TERM_IMAGELESS:
        param.profile = vas::ot::Tracker::PROFILE_ZERO_TERM_IMAGELESS;
        break;
    default:
        printf("Error: Invalid tracker type vas::ot::Tracker\n");
        ETHROW(false, invalid_argument, "Invalid tracker type vas::ot::Tracker");
        return nullptr;
    }

    // Not exposed to external parameter
    param.min_region_ratio_in_boundary =
        kMinRegionRatioInImageBoundary; // Ratio threshold of size: used by zttchist, zttimgless, sttkcfvar, sttimgless

    for (const auto &item : platform_config) {
        (void)item; // resolves ununsed warning when LOG_TRACE is OFF
        TRACE("platform_config[%s] = %s", item.first.c_str(), item.second.c_str());
    }

    ot_impl = new ObjectTracker::Impl(param);
    std::unique_ptr<ObjectTracker> ot(new ObjectTracker(ot_impl));

    TRACE("END");
    return ot;
}

}; // namespace ot
}; // namespace vas
