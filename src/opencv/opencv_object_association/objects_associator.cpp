/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "objects_associator.h"

#include "hungarian_wrap.h"

namespace vas {
namespace ot {

const float kAssociationCostThreshold = 1.0f;

ObjectsAssociator::ObjectsAssociator(bool tracking_per_class, float kRgbHistDistScale, float kNormCenterDistScale,
                                     float kNormShapeDistScale)
    : tracking_per_class_(tracking_per_class), kRgbHistDistScale_(kRgbHistDistScale),
      kNormCenterDistScale_(kNormCenterDistScale), kNormShapeDistScale_(kNormShapeDistScale) {
}

ObjectsAssociator::~ObjectsAssociator() {
}

std::pair<std::vector<bool>, std::vector<int32_t>>
ObjectsAssociator::Associate(const std::vector<Detection> &detections,
                             const std::vector<std::shared_ptr<Tracklet>> &tracklets) {
    std::vector<std::vector<float>> d2t_rgb_dist_table;

    if (!detections[0].feature.empty()) {
        d2t_rgb_dist_table = ComputeRgbDistance(detections, tracklets);
    }

    int32_t n_detections = detections.size();
    int32_t n_tracklets = tracklets.size();

    std::vector<bool> d_is_associated(n_detections, false);
    std::vector<int32_t> t_associated_d_index(n_tracklets, -1);

    // Compute detection-tracklet normalized position distance table
    std::vector<std::vector<float>> d2t_pos_dist_table(n_detections, std::vector<float>(n_tracklets, 1000.0f));
    for (int32_t d = 0; d < n_detections; ++d) {
        // TRACE("input detect(%.0f,%.0f %.0fx%.0f)", detections[d].rect.x, detections[d].rect.y,
        // detections[d].rect.width,
        //      detections[d].rect.height);
        for (int32_t t = 0; t < n_tracklets; ++t) {
            if (tracking_per_class_ && (detections[d].class_label != tracklets[t]->label))
                continue;

            d2t_pos_dist_table[d][t] = NormalizedCenterDistance(detections[d].rect, tracklets[t]->trajectory.back());
        }
    }

    // Compute detection-tracklet normalized shape distance table
    std::vector<std::vector<float>> d2t_shape_dist_table(n_detections, std::vector<float>(n_tracklets, 1000.0f));
    for (int32_t d = 0; d < n_detections; ++d) {
        for (int32_t t = 0; t < n_tracklets; ++t) {
            if (tracking_per_class_ && (detections[d].class_label != tracklets[t]->label))
                continue;

            d2t_shape_dist_table[d][t] = NormalizedShapeDistance(detections[d].rect, tracklets[t]->trajectory.back());
        }
    }

    // Compute detection-tracklet association cost table
    cv::Mat_<float> d2t_cost_table;
    d2t_cost_table.create(static_cast<int32_t>(detections.size()),
                          static_cast<int32_t>(tracklets.size() + detections.size()));
    d2t_cost_table = kAssociationCostThreshold + 1.0f;

    for (int32_t t = 0; t < n_tracklets; ++t) {
        const auto &tracklet = tracklets[t];
        float rgb_hist_dist_scale = kRgbHistDistScale_;

        float const_ratio = 0.95f;
        float norm_center_dist_scale =
            (1.0 - const_ratio) * kNormCenterDistScale_ * tracklet->association_delta_t / 0.033f +
            const_ratio * kNormCenterDistScale_; // adaptive to delta_t
        float norm_shape_dist_scale =
            (1.0 - const_ratio) * kNormShapeDistScale_ * tracklet->association_delta_t / 0.033f +
            const_ratio * kNormShapeDistScale_; // adaptive to delta_t
        float log_term = logf(rgb_hist_dist_scale * norm_center_dist_scale * norm_shape_dist_scale);

        for (int32_t d = 0; d < n_detections; ++d) {
            if (tracking_per_class_ && (detections[d].class_label != tracklets[t]->label))
                continue;

            d2t_cost_table(d, t) = log_term + d2t_pos_dist_table[d][t] / norm_center_dist_scale +
                                   d2t_shape_dist_table[d][t] / norm_shape_dist_scale;

            if (!d2t_rgb_dist_table.empty()) {
                d2t_cost_table(d, t) += d2t_rgb_dist_table[d][t] / kRgbHistDistScale_;
            }
        }
    }

    for (int32_t d = 0; d < n_detections; ++d) {
        d2t_cost_table(d, d + n_tracklets) = kAssociationCostThreshold;
    }

    // Solve detection-tracking association using Hungarian algorithm
    HungarianAlgo hungarian(d2t_cost_table);
    cv::Mat_<uint8_t> d2t_assign_table = hungarian.Solve();

    for (int32_t d = 0; d < n_detections; ++d) {
        for (int32_t t = 0; t < n_tracklets; ++t) {
            if (d2t_assign_table(d, t)) {
                d_is_associated[d] = true;
                t_associated_d_index[t] = d;
                break;
            }
        }
    }

    return std::make_pair(d_is_associated, t_associated_d_index);
}

static float ComputeSimilarity(const cv::Mat &hist1, const cv::Mat &hist2) {
    // PROF_START(PROF_COMPONENTS_OT_SHORTTERM_HIST_SIMILARITY);
    // Bhattacharyya coeff (w/o weights)
    const float eps = 0.0001f;
    const int32_t hist_size = hist1.cols;
    const float *hist_data1 = hist1.ptr<float>();
    const float *hist_data2 = hist2.ptr<float>();
    float corr = 0.0f;
    float sum1 = 0.0f;
    float sum2 = 0.0f;
    for (int32_t i = 0; i < hist_size; ++i) {
        float v1 = hist_data1[i];
        float v2 = hist_data2[i];
        corr += sqrtf(v1 * v2);
        sum1 += v1;
        sum2 += v2;
    }

    if (sum1 > eps && sum2 > eps) {
        return corr / sqrtf(sum1 * sum2);
    } else {
        return 0.0f;
    }
}

std::vector<std::vector<float>>
ObjectsAssociator::ComputeRgbDistance(const std::vector<Detection> &detections,
                                      const std::vector<std::shared_ptr<Tracklet>> &tracklets) {
    int32_t n_detections = detections.size();
    int32_t n_tracklets = tracklets.size();

    // Compute detection-tracklet RGB feature distance table
    std::vector<std::vector<float>> d2t_rgb_dist_table(n_detections, std::vector<float>(n_tracklets, 1000.0f));
    for (int32_t d = 0; d < n_detections; ++d) {
        const auto &d_feature = detections[d].feature;
        for (int32_t t = 0; t < n_tracklets; ++t) {
            if (tracking_per_class_ && (detections[d].class_label != tracklets[t]->label))
                continue;

            // Find best match in rgb feature history
            float min_dist = 1000.0f;
            for (const auto &t_feature : *(tracklets[t]->GetRgbFeatures())) {
                min_dist = std::min(min_dist, 1.0f - ComputeSimilarity(d_feature, t_feature));
            }
            d2t_rgb_dist_table[d][t] = min_dist;
        }
    }

    return d2t_rgb_dist_table;
}

float ObjectsAssociator::NormalizedCenterDistance(const cv::Rect2f &r1, const cv::Rect2f &r2) {
    float normalizer = std::min(0.5f * (r1.width + r1.height), 0.5f * (r2.width + r2.height));

    float r1x = r1.x + 0.5f * r1.width;
    float r1y = r1.y + 0.5f * r1.height;
    float r2x = r2.x + 0.5f * r2.width;
    float r2y = r2.y + 0.5f * r2.height;
    float dx = (r2x - r1x) / normalizer;
    float dy = (r2y - r1y) / normalizer;
    return std::sqrt(dx * dx + dy * dy);
}

float ObjectsAssociator::NormalizedShapeDistance(const cv::Rect2f &r1, const cv::Rect2f &r2) {
    int32_t normalize_w = r1.width;
    int32_t normalize_h = r1.height;

    if (r2.width + r2.height < r1.width + r1.height) {
        normalize_w = r2.width;
        normalize_h = r2.height;
    }

    float dw = (r2.width - r1.width) / normalize_w;
    float dh = (r2.height - r1.height) / normalize_h;
    return std::sqrt(dw * dw + dh * dh);
}

}; // namespace ot
}; // namespace vas
