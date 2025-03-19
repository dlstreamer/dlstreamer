/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "tracker.h"

#include <memory>

namespace vas {
namespace ot {

Tracker::Tracker(vas::ot::Tracker::InitParameters init_param)
    : next_id_(1), frame_count_(0), min_region_ratio_in_boundary_(init_param.min_region_ratio_in_boundary),
      associator_(ObjectsAssociator(init_param.tracking_per_class, init_param.kRgbHistDistScale,
                                    init_param.kNormCenterDistScale, init_param.kNormShapeDistScale)),
      generate_objects(init_param.generate_objects), image_sz(0, 0) {
    if (generate_objects) {
        kMaxAssociationFailCount = 20;
        kMinBirthCount = 1;
    }
}

Tracker::~Tracker() {
}

Tracker *Tracker::CreateInstance(InitParameters init_parameters) {
    return new Tracker(init_parameters);
}

int32_t Tracker::RemoveObject(const int32_t id) {
    if (id == 0)
        return -1;

    for (auto tracklet = tracklets_.begin(); tracklet != tracklets_.end(); ++tracklet) {
        if ((*tracklet)->id == id) {
            tracklet = tracklets_.erase(tracklet);
            return 0;
        }
    }
    return -1;
}

void Tracker::Reset(void) {
    frame_count_ = 0;
    tracklets_.clear();
}

int32_t Tracker::GetFrameCount(void) const {
    return frame_count_;
}

int32_t Tracker::GetNextTrackingID() {
    return next_id_++;
}

void Tracker::IncreaseFrameCount() {
    frame_count_++;
}

void Tracker::ComputeOcclusion() {
    // Compute occlusion ratio
    for (int32_t t0 = 0; t0 < static_cast<int32_t>(tracklets_.size()); ++t0) {
        auto &tracklet0 = tracklets_[t0];
        if (tracklet0->status != ST_TRACKED)
            continue;

        const cv::Rect2f &r0 = tracklet0->trajectory.back();
        float max_occlusion_ratio = 0.0f;
        for (int32_t t1 = 0; t1 < static_cast<int32_t>(tracklets_.size()); ++t1) {
            const auto &tracklet1 = tracklets_[t1];
            if (t0 == t1 || tracklet1->status == ST_LOST)
                continue;

            const cv::Rect2f &r1 = tracklet1->trajectory.back();
            max_occlusion_ratio = std::max(max_occlusion_ratio, (r0 & r1).area() / r0.area()); // different from IoU
        }
        tracklets_[t0]->occlusion_ratio = max_occlusion_ratio;
    }
}

void Tracker::RemoveOutOfBoundTracklets(int32_t input_width, int32_t input_height, bool is_filtered) {
    const cv::Rect2f image_region(0.0f, 0.0f, static_cast<float>(input_width), static_cast<float>(input_height));
    for (auto tracklet = tracklets_.begin(); tracklet != tracklets_.end();) {
        const cv::Rect2f &object_region =
            is_filtered ? (*tracklet)->trajectory_filtered.back() : (*tracklet)->trajectory.back();
        if ((image_region & object_region).area() / object_region.area() <
            min_region_ratio_in_boundary_) { // only 10% is in image boundary
            tracklet = tracklets_.erase(tracklet);
        } else {
            ++tracklet;
        }
    }
}

void Tracker::RemoveDeadTracklets() {
    for (auto tracklet = tracklets_.begin(); tracklet != tracklets_.end();) {
        if ((*tracklet)->status == ST_DEAD) {
            tracklet = tracklets_.erase(tracklet);
        } else {
            ++tracklet;
        }
    }
}

bool Tracker::RemoveOneLostTracklet() {
    for (auto tracklet = tracklets_.begin(); tracklet != tracklets_.end();) {
        if ((*tracklet)->status == ST_LOST) {
            // The first tracklet is the oldest
            tracklet = tracklets_.erase(tracklet);
            return true;
        } else {
            ++tracklet;
        }
    }

    return false;
}

int32_t Tracker::TrackObjects(cv::Size frame_size, const std::vector<Detection> &detections,
                              std::vector<std::shared_ptr<Tracklet>> *tracklets, float delta_t) {
    int32_t input_img_width = frame_size.width;
    int32_t input_img_height = frame_size.height;

    const cv::Rect2f image_boundary(0.0f, 0.0f, static_cast<float>(input_img_width),
                                    static_cast<float>(input_img_height));

    // KALMAN_PREDICTION
    // Predict tracklets state
    for (auto &tracklet : tracklets_) {
        cv::Rect2f predicted_rect = tracklet->kalman_filter->Predict(delta_t);
        tracklet->trajectory.push_back(predicted_rect);
        tracklet->trajectory_filtered.push_back(predicted_rect);
        tracklet->association_delta_t += delta_t;
        // Reset association index every frame for new detection input
        tracklet->association_idx = kNoMatchDetection;
    }

    // is_dead
    if (generate_objects) {
        bool is_dead = false;
        if (image_sz.width != input_img_width || image_sz.height != input_img_height) {
            if (image_sz.width != 0 || image_sz.height != 0) {
                is_dead = true;
            }
            image_sz.width = input_img_width;
            image_sz.height = input_img_height;
        }
        for (auto &tracklet : tracklets_) {
            if (is_dead) {
                tracklet->status = ST_DEAD;
                continue;
            }
            // tracklet->association_delta_t = 0.0f;  // meaning updated by SOT
        }
        if (is_dead) {
            RemoveDeadTracklets();
        }
    }

    // Tracklet-detection association
    int32_t n_detections = static_cast<int32_t>(detections.size());
    int32_t n_tracklets = static_cast<int32_t>(tracklets_.size());
    std::vector<bool> d_is_associated(n_detections, false);
    std::vector<int32_t> t_associated_d_index(n_tracklets, -1);
    if (n_detections > 0) {
        auto result = associator_.Associate(detections, tracklets_);
        d_is_associated = result.first;
        t_associated_d_index = result.second;
    }

    // Update tracklets' state
    if (n_detections == 0 && generate_objects) {
        for (int32_t t = 0; t < static_cast<int32_t>(tracklets_.size()); ++t) {
            auto &tracklet = tracklets_[t];
            // Always change ST_NEW to ST_TRACKED: no feature tracking from previous detection input.
            if (tracklet->status == ST_NEW) {
                tracklet->status = ST_TRACKED;
            }

            if (tracklet->status == ST_TRACKED) {
                if (tracklet->age > kMaxOutdatedCountInTracked) {
                    tracklet->status = ST_LOST;
                    tracklet->association_fail_count = 0;
                    tracklet->age = 0;
                } else {
                    tracklet->trajectory_filtered.back() =
                        tracklet->kalman_filter->Correct(tracklet->trajectory.back());
                }
            }

            if (tracklet->status == ST_LOST) {
                if (tracklet->age >= kMaxOutdatedCountInLost) {
                    // # association fail > threshold while missing -> DEAD
                    tracklet->status = ST_DEAD;
                }
            }
        }
    } else { // (n_detections > 0)
        for (int32_t t = 0; t < n_tracklets; ++t) {
            auto &tracklet = tracklets_[t];
            if (t_associated_d_index[t] >= 0) {
                tracklet->association_delta_t = 0.0f;
                int32_t associated_d_index = t_associated_d_index[t];
                const cv::Rect2f &d_bounding_box = detections[associated_d_index].rect & image_boundary;

                // Apply associated detection to tracklet
                tracklet->association_idx = detections[associated_d_index].index;
                tracklet->association_fail_count = 0;
                tracklet->age = 0;
                tracklet->label = detections[associated_d_index].class_label;

                if (tracklet->status == ST_NEW) {
                    tracklet->trajectory.back() = d_bounding_box;
                    tracklet->trajectory_filtered.back() =
                        tracklet->kalman_filter->Correct(tracklet->trajectory.back());
                    tracklet->birth_count += 1;
                    if (tracklet->birth_count >= kMinBirthCount) {
                        tracklet->status = ST_TRACKED;
                    }
                } else if (tracklet->status == ST_TRACKED) {
                    tracklet->trajectory.back() = d_bounding_box;
                    tracklet->trajectory_filtered.back() =
                        tracklet->kalman_filter->Correct(tracklet->trajectory.back());
                } else if (tracklet->status == ST_LOST) {
                    tracklet->RenewTrajectory(d_bounding_box);
                    tracklet->status = ST_TRACKED;
                } else // Association failure
                {
                    tracklet->association_fail_count++;
                    if (tracklet->status == ST_NEW) {
                        tracklet->status = ST_DEAD; // regard non-consecutive association as false alarm
                    } else if (tracklet->status == ST_TRACKED) {
                        if (tracklet->association_fail_count > kMaxAssociationLostCount) {
                            // # association fail > threshold while tracking -> MISSING
                            tracklet->status = ST_LOST;
                            tracklet->association_fail_count = 0;
                            tracklet->age = 0;
                        }
                    } else if (tracklet->status == ST_LOST) {
                        if (tracklet->association_fail_count > kMaxAssociationFailCount) {
                            // # association fail > threshold while missing -> DEAD
                            tracklet->status = ST_DEAD;
                        }
                    }
                }
            }
        }
    }

    ComputeOcclusion();

    // Update tracklets' model with new feature
    for (int32_t t = 0; t < static_cast<int32_t>(tracklets_.size()); ++t) {
        int32_t associated_d_index = t_associated_d_index[t];
        if (associated_d_index >= 0) {
            auto &feature = detections[associated_d_index].feature;
            if (!feature.empty()) {
                auto &tracklet = tracklets_[t];
                if (tracklet->status == ST_NEW) {
                    tracklet->GetRgbFeatures()->push_back(feature);
                } else if (tracklet->status == ST_TRACKED) {
                    if (tracklet->occlusion_ratio < kMaxOcclusionRatioForModelUpdate) {
                        tracklet->GetRgbFeatures()->push_back(feature);
                    }
                } else if (tracklet->status == ST_LOST) {
                    tracklet->GetRgbFeatures()->push_back(feature);
                }
            }
        }
    }

    // Register remaining detections as new objects
    for (size_t d = 0; d < detections.size(); d++) {
        if (d_is_associated[d] == false) {
            // if (static_cast<int32_t>(tracklets_.size()) >= max_objects_ && max_objects_ != -1)
            //    continue;

            auto tracklet = std::make_unique<ShortTermImagelessTracklet>();

            tracklet->status = ST_NEW;
            tracklet->id = GetNextTrackingID();
            tracklet->label = detections[d].class_label;
            tracklet->association_idx = detections[d].index;

            const cv::Rect2f &bounding_box = detections[d].rect & image_boundary;
            tracklet->InitTrajectory(bounding_box);
            tracklet->kalman_filter.reset(new KalmanFilterNoOpencv(bounding_box));
            if (!detections[d].feature.empty())
                tracklet->rgb_features.push_back(detections[d].feature);

            tracklets_.push_back(std::move(tracklet));
        }
    }

    RemoveDeadTracklets();
    RemoveOutOfBoundTracklets(input_img_width, input_img_height);
    TrimTrajectories();

    *tracklets = tracklets_;

    // Increase age
    for (auto &tracklet : tracklets_) {
        tracklet->age++;
    }

    IncreaseFrameCount();
    return 0;
}

void Tracker::TrimTrajectories() {
    for (auto &tracklet : tracklets_) {
        auto &trajectory = tracklet->trajectory;
        while (trajectory.size() > kMaxTrajectorySize) {
            trajectory.pop_front();
        }

        //
        auto &trajectory_filtered = tracklet->trajectory_filtered;
        while (trajectory_filtered.size() > kMaxTrajectorySize) {
            trajectory_filtered.pop_front();
        }

        //
        auto &rgb_features = *(tracklet->GetRgbFeatures());
        while (rgb_features.size() > kMaxRgbFeatureHistory) {
            rgb_features.pop_front();
        }
    }
}

}; // namespace ot
}; // namespace vas
