/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "itracker.h"
#include <dlstreamer/base/memory_mapper.h>
#include <dlstreamer/context.h>
#include <gva_utils.h>

#include <opencv2/opencv.hpp>
#include <openvino/openvino.hpp>

#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

namespace DeepSortWrapper {

// Deep SORT specific parameters
constexpr float DEFAULT_MAX_IOU_DISTANCE = 0.7f;    // Maximum IoU distance threshold for matching
constexpr int DEFAULT_MAX_AGE = 30;                 // Maximum number of missed frames before track is deleted
constexpr int DEFAULT_N_INIT = 3;                   // Number of consecutive hits required to confirm a track
constexpr float DEFAULT_MAX_COSINE_DISTANCE = 0.2f; // Maximum cosine distance for appearance matching
constexpr int DEFAULT_NN_BUDGET = 100;
constexpr int DEFAULT_FEATURES_VECTOR_SIZE_128 = 128;

// Track states
enum class TrackState { Tentative = 1, Confirmed = 2, Deleted = 3 };

// Detection structure for Deep SORT
struct Detection {
    cv::Rect_<float> bbox;
    float confidence;
    std::vector<float> feature;
    int class_id;

    Detection(const cv::Rect_<float> &bbox, float confidence, const std::vector<float> &feature, int class_id = -1)
        : bbox(bbox), confidence(confidence), feature(feature), class_id(class_id) {
    }
};

// Track structure for Deep SORT
class Track {
  public:
    Track(const cv::Rect_<float> &bbox, int track_id, int n_init, int max_age, const std::vector<float> &feature);

    void update(const Detection &detection);
    void mark_missed();
    bool is_tentative() const {
        return state_ == TrackState::Tentative;
    }
    bool is_confirmed() const {
        return state_ == TrackState::Confirmed;
    }
    bool is_deleted() const {
        return state_ == TrackState::Deleted;
    }

    cv::Rect_<float> to_bbox() const;
    int track_id() const {
        return track_id_;
    }
    int time_since_update() const {
        return time_since_update_;
    }

    // Feature management
    void add_feature(const std::vector<float> &feature);
    const std::deque<std::vector<float>> &features() const {
        return features_;
    }

    std::string state_str() const {
        switch (state_) {
        case TrackState::Tentative:
            return "Tentative";
        case TrackState::Confirmed:
            return "Confirmed";
        case TrackState::Deleted:
            return "Deleted";
        default:
            return "Unknown";
        }
    }

  private:
    // Kalman filter state - OpenCV implementation
    cv::Mat mean_;
    cv::Mat covariance_;

    int track_id_;
    int hits_;
    int age_;
    int time_since_update_;
    TrackState state_;

    // Parameters
    int n_init_;
    int max_age_;
    int nn_budget_;

    // Feature storage for cosine distance calculation
    std::deque<std::vector<float>> features_;

    void initiate(const cv::Rect_<float> &bbox);
    void predict();
};

// Deep SORT feature extractor using OpenVINO
class FeatureExtractor {
  public:
    FeatureExtractor(const std::string &model_path, const std::string &device = "CPU");
    ~FeatureExtractor() = default;

    std::vector<float> extract(const cv::Mat &image, const cv::Rect &bbox);
    std::vector<std::vector<float>> extract_batch(const cv::Mat &image, const std::vector<cv::Rect> &bboxes);

  private:
    ov::Core core_;
    ov::CompiledModel compiled_model_;
    ov::InferRequest infer_request_;

    int input_height_;
    int input_width_;

    cv::Mat preprocess(const cv::Mat &image);
    std::vector<float> postprocess(const ov::Tensor &output);
};

// Deep SORT tracker implementation
class DeepSortTracker : public ITracker {
  public:
    // Constructor for feature extraction using provided model
    DeepSortTracker(const std::string &feature_model_path, const std::string &device = "CPU",
                    float max_iou_distance = DEFAULT_MAX_IOU_DISTANCE, int max_age = DEFAULT_MAX_AGE,
                    int n_init = DEFAULT_N_INIT, float max_cosine_distance = DEFAULT_MAX_COSINE_DISTANCE,
                    int nn_budget = DEFAULT_NN_BUDGET, dlstreamer::MemoryMapperPtr mapper = nullptr);

    // Constructor for using pre-extracted features from gvainference
    DeepSortTracker(float max_iou_distance = DEFAULT_MAX_IOU_DISTANCE, int max_age = DEFAULT_MAX_AGE,
                    int n_init = DEFAULT_N_INIT, float max_cosine_distance = DEFAULT_MAX_COSINE_DISTANCE,
                    int nn_budget = DEFAULT_NN_BUDGET, dlstreamer::MemoryMapperPtr mapper = nullptr);

    ~DeepSortTracker() override = default;

    void track(dlstreamer::FramePtr buffer, GVA::VideoFrame &frame_meta) override;

    void do_color_space_conversion(cv::Mat &image, cv::Mat &raw_image, dlstreamer::FramePtr sys_buffer);

  private:
    // Deep SORT algorithm components
    std::unique_ptr<FeatureExtractor> feature_extractor_; // nullptr when using pre-extracted features
    std::vector<std::unique_ptr<Track>> tracks_;
    int next_id_;

    // Parameters
    float max_iou_distance_;
    int max_age_;
    int n_init_;
    float max_cosine_distance_;
    int nn_budget_;

    // Memory mapper for buffer access
    dlstreamer::MemoryMapperPtr buffer_mapper_;

    // Helper methods
    std::vector<Detection> convert_detections(const cv::Mat &image, const std::vector<GVA::RegionOfInterest> &regions);
    void associate_detections_to_tracks(const std::vector<Detection> &detections,
                                        std::vector<std::pair<int, int>> &matches, std::vector<int> &unmatched_dets,
                                        std::vector<int> &unmatched_trks);
    float calculate_cosine_distance(const std::vector<float> &feat1, const std::vector<float> &feat2);
    float calculate_iou(const cv::Rect_<float> &bbox1, const cv::Rect_<float> &bbox2);

    // Hungarian algorithm for assignment
    void hungarian_assignment(const std::vector<std::vector<float>> &cost_matrix,
                              std::vector<std::pair<int, int>> &assignments);
    void hungarian_assignment_greedy(const std::vector<std::vector<float>> &cost_matrix,
                                     std::vector<std::pair<int, int>> &assignments);
};

} // namespace DeepSortWrapper