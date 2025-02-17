/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vas/components/ot/zero_term_chist_tracker.h"

#include "vas/common/exception.h"
#include "vas/components/ot/prof_def.h"
#include <cmath>
#include <memory>

namespace vas {
namespace ot {

// const int32_t kMaxAssociationFailCount = 600;  // about 20 seconds
const int32_t kMaxAssociationFailCount = 120; // about 4 seconds
const int32_t kMaxTrajectorySize = 30;

const float kMaxOcclusionRatioForModelUpdate = 0.4f;

const int32_t kSrgbCanonicalPatchSize = 64;
const int32_t kRgbSpatialBinSize = 32;
const int32_t kSrgbSpatialBinStride = 32;
const int32_t kSrgbRgbBinSize = 32;

const int32_t kMaxRgbFeatureHistory = 1;
const int32_t kMinBirthCount = 3;

/**
 *
 * ZeroTermChistTracker
 *
 **/
ZeroTermChistTracker::ZeroTermChistTracker(vas::ot::Tracker::InitParameters init_param)
    : Tracker(init_param.max_num_objects, init_param.min_region_ratio_in_boundary, init_param.format,
              init_param.tracking_per_class),
      rgb_hist_(kSrgbCanonicalPatchSize, kRgbSpatialBinSize, kSrgbSpatialBinStride, kSrgbRgbBinSize) {
    TRACE(" - Created tracker = ZeroTermChistTracker");
}

ZeroTermChistTracker::~ZeroTermChistTracker() {
}

int32_t ZeroTermChistTracker::TrackObjects(const cv::Mat &mat, const std::vector<Detection> &detections,
                                           std::vector<std::shared_ptr<Tracklet>> *tracklets, float delta_t) {
    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_RUN_TRACKER);

    int32_t input_img_width = mat.cols;
    int32_t input_img_height = mat.rows;

    if (input_image_format_ == vas::ColorFormat::NV12 || input_image_format_ == vas::ColorFormat::I420) {
        input_img_height = mat.rows / 3 * 2;
    }
    const cv::Rect2f image_boundary(0.0f, 0.0f, static_cast<float>(input_img_width),
                                    static_cast<float>(input_img_height));

    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_KALMAN_PREDICTION);
    // Predict tracklets state
    for (auto &tracklet : tracklets_) {
        auto zttchist_tracklet = std::dynamic_pointer_cast<ZeroTermChistTracklet>(tracklet);
        cv::Rect2f predicted_rect = zttchist_tracklet->kalman_filter->Predict(delta_t);
        zttchist_tracklet->predicted = predicted_rect;
        zttchist_tracklet->trajectory.push_back(predicted_rect);
        zttchist_tracklet->trajectory_filtered.push_back(predicted_rect);
        zttchist_tracklet->association_delta_t += delta_t;
        // Reset matching association index every frame
        zttchist_tracklet->association_idx = kNoMatchDetection;
    }

    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_KALMAN_PREDICTION);

    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_RUN_ASSOCIATION);
    int32_t n_detections = static_cast<int32_t>(detections.size());
    int32_t n_tracklets = static_cast<int32_t>(tracklets_.size());

    std::vector<bool> d_is_associated(n_detections, false);
    std::vector<int32_t> t_associated_d_index(n_tracklets, -1);

    std::vector<cv::Mat> d_rgb_features;

    if (detections.size() > 0) {
        // Compute RGB features for new detections
        for (const auto &detection : detections) {
            cv::Mat rgb_feature;
            if (input_image_format_ == vas::ColorFormat::NV12) {
                YuvImage img(mat, YuvImage::FMT_NV12, frame_count_);
                rgb_hist_.ComputeFromNv12(img, detection.rect & image_boundary,
                                          &rgb_feature); // YuvImage container to feature
            } else if (input_image_format_ == vas::ColorFormat::BGR) {
                cv::Mat roi_patch = mat(detection.rect & image_boundary);
                rgb_hist_.Compute(roi_patch, &rgb_feature);
            } else if (input_image_format_ == vas::ColorFormat::BGRX) {
                cv::Mat roi_patch = mat(detection.rect & image_boundary);
                rgb_hist_.ComputeFromBgra32(roi_patch, &rgb_feature);
            } else if (input_image_format_ == vas::ColorFormat::I420) {
                YuvImage img(mat, YuvImage::FMT_I420, frame_count_);
                rgb_hist_.ComputeFromI420(img, detection.rect & image_boundary,
                                          &rgb_feature); // YuvImage container to feature
            }

            d_rgb_features.push_back(rgb_feature);
        }

        auto result = associator_.Associate(detections, tracklets_, &d_rgb_features);
        d_is_associated = result.first;
        t_associated_d_index = result.second;
    }
    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_RUN_ASSOCIATION);

    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_UPDATE_STATUS);
    // Update tracklets' state
    for (int32_t t = 0; t < n_tracklets; ++t) {
        auto &tracklet = tracklets_[t];
        if (t_associated_d_index[t] >= 0) {
            tracklet->association_delta_t = 0.0f;

            int32_t associated_d_index = t_associated_d_index[t];
            const cv::Rect2f &d_bounding_box = detections[associated_d_index].rect;

            // Apply associated detection to tracklet
            tracklet->association_fail_count = 0;
            tracklet->association_idx = detections[associated_d_index].index;
            tracklet->label = detections[associated_d_index].class_label;

            auto zttchist_tracklet = std::dynamic_pointer_cast<ZeroTermChistTracklet>(tracklet);
            if (!zttchist_tracklet)
                continue;

            if (zttchist_tracklet->status == ST_NEW) {
                zttchist_tracklet->trajectory.back() = d_bounding_box;
                zttchist_tracklet->trajectory_filtered.back() =
                    zttchist_tracklet->kalman_filter->Correct(zttchist_tracklet->trajectory.back());
                zttchist_tracklet->birth_count += 1;
                if (zttchist_tracklet->birth_count >= kMinBirthCount) {
                    zttchist_tracklet->status = ST_TRACKED;
                }
            } else if (zttchist_tracklet->status == ST_TRACKED) {
                zttchist_tracklet->trajectory.back() = d_bounding_box;
                zttchist_tracklet->trajectory_filtered.back() =
                    zttchist_tracklet->kalman_filter->Correct(zttchist_tracklet->trajectory.back());
            } else if (zttchist_tracklet->status == ST_LOST) {
                zttchist_tracklet->RenewTrajectory(d_bounding_box);
                zttchist_tracklet->kalman_filter = std::make_unique<KalmanFilterNoOpencv>(d_bounding_box);
                zttchist_tracklet->status = ST_TRACKED;
            }
        } else {
            if (tracklet->status == ST_NEW) {
                tracklet->status = ST_DEAD; // regard non-consecutive association as false alarm
            } else if (tracklet->status == ST_TRACKED) {
                tracklet->status = ST_LOST;
                tracklet->association_fail_count = 0;
            } else if (tracklet->status == ST_LOST) {
                if (++tracklet->association_fail_count >= kMaxAssociationFailCount) {
                    // # association fail > threshold while missing -> DEAD
                    tracklet->status = ST_DEAD;
                }
            }
        }
    }
    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_UPDATE_STATUS);

    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_COMPUTE_OCCLUSION);
    ComputeOcclusion();
    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_COMPUTE_OCCLUSION);

    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_UPDATE_MODEL);
    // Update tracklets' model
    for (int32_t t = 0; t < static_cast<int32_t>(tracklets_.size()); ++t) {
        if (t_associated_d_index[t] >= 0) {
            auto &tracklet = tracklets_[t];

            int32_t associated_d_index = t_associated_d_index[t];
            const cv::Mat &d_rgb_feature = d_rgb_features[associated_d_index];

            if (tracklet->status == ST_NEW) {
                tracklet->GetRgbFeatures()->push_back(d_rgb_feature);
            } else if (tracklet->status == ST_TRACKED) {
                if (tracklet->occlusion_ratio < kMaxOcclusionRatioForModelUpdate) {
                    tracklet->GetRgbFeatures()->push_back(d_rgb_feature);
                }
            } else if (tracklet->status == ST_LOST) {
                tracklet->GetRgbFeatures()->push_back(d_rgb_feature);
            }
        }
    }
    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_UPDATE_MODEL);

    PROF_START(PROF_COMPONENTS_OT_ZEROTERM_REGISTER_OBJECT);
    // Register remaining detections as new objects
    for (int32_t d = 0; d < static_cast<int32_t>(detections.size()); ++d) {
        if (d_is_associated[d] == false) {
            if (static_cast<int32_t>(tracklets_.size()) >= max_objects_ && max_objects_ != -1)
                continue;

            std::unique_ptr<ZeroTermChistTracklet> tracklet = std::make_unique<ZeroTermChistTracklet>();

            tracklet->status = ST_NEW;

            tracklet->id = GetNextTrackingID();

            tracklet->label = detections[d].class_label;
            tracklet->association_idx = detections[d].index;

            const cv::Rect2f &bounding_box = detections[d].rect;
            tracklet->InitTrajectory(bounding_box);
            tracklet->kalman_filter = std::make_unique<KalmanFilterNoOpencv>(bounding_box);
            tracklet->rgb_features.push_back(d_rgb_features[d]);

            tracklets_.push_back(std::move(tracklet));
        }
    }
    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_REGISTER_OBJECT);

    RemoveDeadTracklets();
    RemoveOutOfBoundTracklets(input_img_width, input_img_height);
    TrimTrajectories();

    *tracklets = tracklets_;

    IncreaseFrameCount();
    PROF_END(PROF_COMPONENTS_OT_ZEROTERM_RUN_TRACKER);
    return 0;
}

void ZeroTermChistTracker::TrimTrajectories() {
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
