/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "deep_sort_tracker.h"
#include "mapped_mat.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace DeepSortWrapper {

// Track implementation

/**
 * @brief Constructs a new Track with initial detection data and Kalman filter state
 */
Track::Track(const cv::Rect_<float> &bbox, int track_id, int n_init, int max_age, const std::vector<float> &feature)
    : track_id_(track_id), hits_(1), age_(1), time_since_update_(0), state_(TrackState::Tentative), n_init_(n_init),
      max_age_(max_age), nn_budget_(DEFAULT_NN_BUDGET) {
    initiate(bbox);
    add_feature(feature);
}

/**
 * @brief Initialize Kalman filter state and covariance matrix from first detection bbox
 */
void Track::initiate(const cv::Rect_<float> &bbox) {
    // Initialize Kalman filter with 8-dimensional state space (x, y, aspect_ratio, height, vx, vy, va, vh)
    mean_ = cv::Mat::zeros(8, 1, CV_32F);
    mean_.at<float>(0) = bbox.x + bbox.width / 2.0f;  // center_x
    mean_.at<float>(1) = bbox.y + bbox.height / 2.0f; // center_y
    mean_.at<float>(2) = bbox.width / bbox.height;    // aspect_ratio
    mean_.at<float>(3) = bbox.height;                 // height

    // Initialize covariance matrix
    covariance_ = cv::Mat::eye(8, 8, CV_32F);
    float std_weight_position = 1.0f / 20.0f;
    float std_weight_velocity = 1.0f / 160.0f;

    covariance_.at<float>(0, 0) = 2.0f * std_weight_position * bbox.height;
    covariance_.at<float>(1, 1) = 2.0f * std_weight_position * bbox.height;
    covariance_.at<float>(2, 2) = 1e-2;
    covariance_.at<float>(3, 3) = 2.0f * std_weight_position * bbox.height;
    covariance_.at<float>(4, 4) = 10.0f * std_weight_velocity * bbox.height;
    covariance_.at<float>(5, 5) = 10.0f * std_weight_velocity * bbox.height;
    covariance_.at<float>(6, 6) = 1e-5;
    covariance_.at<float>(7, 7) = 10.0f * std_weight_velocity * bbox.height;
}

/**
 * @brief Predict next state using Kalman filter motion model (constant velocity)
 */
void Track::predict() {
    // OpenCV-based implementation
    // State transition matrix
    cv::Mat F = cv::Mat::eye(8, 8, CV_32F);
    F.at<float>(0, 4) = 1.0f; // x += vx
    F.at<float>(1, 5) = 1.0f; // y += vy
    F.at<float>(2, 6) = 1.0f; // aspect_ratio += va
    F.at<float>(3, 7) = 1.0f; // height += vh

    // Predict state
    mean_ = F * mean_;

    // Process noise
    cv::Mat Q = cv::Mat::eye(8, 8, CV_32F);
    float std_weight_position = 1.0f / 20.0f;
    float std_weight_velocity = 1.0f / 160.0f;
    float height = mean_.at<float>(3);

    Q.at<float>(0, 0) = std::pow(std_weight_position * height, 2);
    Q.at<float>(1, 1) = std::pow(std_weight_position * height, 2);
    Q.at<float>(2, 2) = 1e-2;
    Q.at<float>(3, 3) = std::pow(std_weight_position * height, 2);
    Q.at<float>(4, 4) = std::pow(std_weight_velocity * height, 2);
    Q.at<float>(5, 5) = std::pow(std_weight_velocity * height, 2);
    Q.at<float>(6, 6) = 1e-5;
    Q.at<float>(7, 7) = std::pow(std_weight_velocity * height, 2);

    // Update covariance
    covariance_ = F * covariance_ * F.t() + Q;
}

/**
 * @brief Update track state with matched detection using Kalman filter correction step
 */
void Track::update(const Detection &detection) {
    predict();

    // OpenCV-based implementation
    // Measurement model (we observe x, y, aspect_ratio, height)
    cv::Mat H = cv::Mat::zeros(4, 8, CV_32F);
    H.at<float>(0, 0) = 1.0f; // observe x
    H.at<float>(1, 1) = 1.0f; // observe y
    H.at<float>(2, 2) = 1.0f; // observe aspect_ratio
    H.at<float>(3, 3) = 1.0f; // observe height

    // Measurement noise
    cv::Mat R = cv::Mat::eye(4, 4, CV_32F);
    float std_weight_position = 1.0f / 20.0f;
    float height = detection.bbox.height;

    R.at<float>(0, 0) = std::pow(std_weight_position * height, 2);
    R.at<float>(1, 1) = std::pow(std_weight_position * height, 2);
    R.at<float>(2, 2) = 1e-1;
    R.at<float>(3, 3) = std::pow(std_weight_position * height, 2);

    // Measurement vector
    cv::Mat z = cv::Mat::zeros(4, 1, CV_32F);
    z.at<float>(0) = detection.bbox.x + detection.bbox.width / 2.0f;
    z.at<float>(1) = detection.bbox.y + detection.bbox.height / 2.0f;
    z.at<float>(2) = detection.bbox.width / detection.bbox.height;
    z.at<float>(3) = detection.bbox.height;

    // Kalman update
    cv::Mat S = H * covariance_ * H.t() + R;
    cv::Mat K = covariance_ * H.t() * S.inv();
    cv::Mat y = z - H * mean_;

    mean_ = mean_ + K * y;
    covariance_ = covariance_ - K * H * covariance_;

    add_feature(detection.feature);

    hits_++;
    time_since_update_ = 0;

    if (state_ == TrackState::Tentative && hits_ >= n_init_) {
        state_ = TrackState::Confirmed;
    }
}

/**
 * @brief Mark track as missed (no detection match) and update state/age counters
 */
void Track::mark_missed() {
    if (state_ == TrackState::Tentative) {
        // For tentative tracks, delete only after max_age misses, not immediately
        if (time_since_update_ >= max_age_) {
            state_ = TrackState::Deleted;
        }
    } else if (time_since_update_ >= max_age_) {
        state_ = TrackState::Deleted;
    }
    time_since_update_++;
    age_++;
}

/**
 * @brief Convert Kalman filter state back to bounding box coordinates
 */
cv::Rect_<float> Track::to_bbox() const {
    // Original OpenCV-based implementation
    float center_x = mean_.at<float>(0);
    float center_y = mean_.at<float>(1);
    float aspect_ratio = mean_.at<float>(2);
    float height = mean_.at<float>(3);
    float width = aspect_ratio * height;

    return cv::Rect_<float>(center_x - width / 2.0f, center_y - height / 2.0f, width, height);
}

/**
 * @brief Add new feature vector to track's feature history (with budget limit)
 */
void Track::add_feature(const std::vector<float> &feature) {
    features_.push_back(feature);
    if (features_.size() > static_cast<size_t>(nn_budget_)) {
        features_.pop_front();
    }
}

// FeatureExtractor implementation

/**
 * @brief Initialize OpenVINO feature extraction model for Deep SORT re-identification
 */
FeatureExtractor::FeatureExtractor(const std::string &model_path, const std::string &device) {
    // Load OpenVINO model
    auto model = core_.read_model(model_path);
    compiled_model_ = core_.compile_model(model, device);
    infer_request_ = compiled_model_.create_infer_request();
    GST_INFO("Model %s loaded and compiled successfully for device %s", model_path.c_str(), device.c_str());

    // Get input dimensions - handle different model types
    auto input_port = compiled_model_.input();

    if (model_path.find("mars") != std::string::npos) {
        // MARS model: expects [1, 3, 128, 64] - NCHW format
        const auto &partial_shape = input_port.get_partial_shape();
        const auto &input_shape =
            partial_shape.is_dynamic() ? partial_shape.get_min_shape() : partial_shape.get_shape();

        if (input_shape.size() < 4) {
            throw std::runtime_error("MARS model input must have at least 4 dimensions (NCHW)");
        }

        input_height_ = input_shape[2]; // Height = 128
        input_width_ = input_shape[3];  // Width = 64

        GST_INFO("MARS model detected: input shape [%lu, %lu, %lu, %lu], using H=%d, W=%d", input_shape[0],
                 input_shape[1], input_shape[2], input_shape[3], input_height_, input_width_);

    } else {
        GST_ERROR("Unsupported model provided for Deep SORT feature extractor: %s. Expecting MARS model with input "
                  "shape [1, 3, 128, 64]",
                  model_path.c_str());
        throw std::runtime_error("Unsupported model provided for Deep SORT feature extractor: " + model_path);
    }

    // Validate extracted dimensions
    if (input_height_ <= 0 || input_width_ <= 0) {
        throw std::runtime_error("Invalid input dimensions detected from model: " + std::to_string(input_width_) + "x" +
                                 std::to_string(input_height_));
    }
}

/**
 * @brief Extract feature vector from single bounding box region using OpenVINO inference
 */
std::vector<float> FeatureExtractor::extract(const cv::Mat &image, const cv::Rect &bbox) {
    // Validate bbox is completely within image bounds
    cv::Rect image_bounds(0, 0, image.cols, image.rows);

    // Check if bbox is valid and completely within image
    if (bbox.area() == 0) {
        GST_WARNING("Invalid bbox (zero area), returning zero feature");
        return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
    }

    if (bbox.x < 0 || bbox.y < 0 || bbox.x + bbox.width > image.cols || bbox.y + bbox.height > image.rows) {
        GST_WARNING("Bbox extends beyond image bounds (%dx%d at (%d,%d) in %dx%d image), returning zero feature",
                    bbox.width, bbox.height, bbox.x, bbox.y, image.cols, image.rows);
        return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
    }

    try {
        cv::Mat roi = image(bbox);
        if (roi.empty()) {
            GST_WARNING("Empty ROI, returning zero feature");
            return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
        }

        cv::Mat preprocessed = preprocess(roi);
        if (preprocessed.empty()) {
            GST_ERROR("Preprocessing failed, returning zero feature");
            return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
        }

        // Set input tensor
        auto input_tensor = infer_request_.get_input_tensor();

        // Bounds check
        size_t expected_size = preprocessed.total();
        size_t tensor_size = input_tensor.get_size();

        if (expected_size != tensor_size) {
            GST_ERROR("Size mismatch: preprocessed=%lu, tensor=%lu", expected_size, tensor_size);
            return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
        }

        if (input_tensor.get_element_type() == ov::element::f32) {
            // Model uses FP32 (or INT8) - direct copy
            float *input_data = input_tensor.data<float>();
            std::memcpy(input_data, preprocessed.data, expected_size * sizeof(float));

        } else {
            GST_ERROR("Unsupported tensor data type: %s. Supporting only FP32 for now.",
                      input_tensor.get_element_type().get_type_name().c_str());
            return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
        }

        // Run inference
        infer_request_.infer();

        // Get output
        auto output_tensor = infer_request_.get_output_tensor();

        std::vector<float> feature = postprocess(output_tensor);

        return feature;

    } catch (const std::exception &e) {
        GST_ERROR("Exception in feature extraction: %s", e.what());
        return std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f);
    }
}

/**
 * @brief Extract features from multiple bounding boxes in batch processing
 */
std::vector<std::vector<float>> FeatureExtractor::extract_batch(const cv::Mat &image,
                                                                const std::vector<cv::Rect> &bboxes) {
    std::vector<std::vector<float>> features;
    features.reserve(bboxes.size());

    for (const auto &bbox : bboxes) {
        features.push_back(extract(image, bbox));
    }

    return features;
}

/**
 * @brief Preprocess image ROI for neural network input (resize, normalize, HWC->CHW)
 */
cv::Mat FeatureExtractor::preprocess(const cv::Mat &image) {

    // Basic safety check
    if (image.empty()) {
        GST_ERROR("Input image is empty");
        return cv::Mat();
    }

    // Validate input has expected channels for RGB input
    if (image.channels() != 3) {
        GST_ERROR("Deep SORT feature extractor expects 3-channel RGB input, got %d channels", image.channels());
        return cv::Mat();
    }

    try {
        cv::Mat resized, normalized;
        cv::resize(image, resized, cv::Size(input_width_, input_height_));

        if (resized.empty()) {
            GST_ERROR("Resize operation failed");
            return cv::Mat();
        }

        resized.convertTo(normalized, CV_32F, 1.0f / 255.0f);
        if (normalized.empty()) {
            GST_ERROR("Normalization failed");
            return cv::Mat();
        }

        // Manual pixel-by-pixel copying with maximum safety
        int channels = normalized.channels();
        cv::Mat result = cv::Mat::zeros(1, channels * input_height_ * input_width_, CV_32F);
        float *result_data = result.ptr<float>();

        if (!result_data) {
            GST_ERROR("Failed to get result data pointer");
            return cv::Mat();
        }

        // Convert HWC to CHW format using memcpy for efficiency
        if (normalized.isContinuous()) {
            // Direct memory copy for continuous data
            size_t pixel_count = input_height_ * input_width_;
            float *src_data = normalized.ptr<float>();

            for (int c = 0; c < channels; ++c) {
                for (size_t i = 0; i < pixel_count; ++i) {
                    result_data[c * pixel_count + i] = src_data[i * channels + c];
                }
            }
        } else {
            GST_ERROR("Image data is not continuous, cannot use optimized copy");
            return cv::Mat();
        }

        return result;
    } catch (const cv::Exception &e) {
        GST_ERROR("OpenCV exception in preprocess: %s", e.what());
        return cv::Mat();
    } catch (const std::exception &e) {
        GST_ERROR("Standard exception in preprocess: %s", e.what());
        return cv::Mat();
    }
}

/**
 * @brief Post-process neural network output tensor to normalized feature vector
 */
std::vector<float> FeatureExtractor::postprocess(const ov::Tensor &output) {
    const float *output_data = output.data<const float>();
    size_t feature_size = output.get_size();

    std::vector<float> feature(output_data, output_data + feature_size);

    // L2 normalize the feature
    float norm = std::sqrt(std::inner_product(feature.begin(), feature.end(), feature.begin(), 0.0f));
    if (norm > 0.0f) {
        for (float &f : feature) {
            f /= norm;
        }
    }

    return feature;
}

// DeepSortTracker implementation

/**
 * @brief Initialize Deep SORT tracker with feature extractor and tracking parameters
 */
DeepSortTracker::DeepSortTracker(const std::string &feature_model_path, const std::string &device,
                                 float max_iou_distance, int max_age, int n_init, float max_cosine_distance,
                                 int nn_budget, dlstreamer::MemoryMapperPtr mapper)
    : feature_extractor_(std::make_unique<FeatureExtractor>(feature_model_path, device)), next_id_(1),
      max_iou_distance_(max_iou_distance), max_age_(max_age), n_init_(n_init),
      max_cosine_distance_(max_cosine_distance), nn_budget_(nn_budget), buffer_mapper_(std::move(mapper)) {

    GST_INFO("DeepSortTracker initialized with OpenCV KALMAN FILTER and FeatureExtractor: max_iou_distance=%.3f, "
             "max_age=%d, n_init=%d, "
             "max_cosine_distance=%.3f",
             max_iou_distance_, max_age_, n_init_, max_cosine_distance_);
}

/**
 * @brief Initialize Deep SORT tracker with tracking parameters (features from gvainference)
 */
DeepSortTracker::DeepSortTracker(float max_iou_distance, int max_age, int n_init, float max_cosine_distance,
                                 int nn_budget, dlstreamer::MemoryMapperPtr mapper)
    : feature_extractor_(nullptr), next_id_(1), max_iou_distance_(max_iou_distance), max_age_(max_age), n_init_(n_init),
      max_cosine_distance_(max_cosine_distance), nn_budget_(nn_budget), buffer_mapper_(std::move(mapper)) {

    GST_INFO("DeepSortTracker initialized with OpenCV KALMAN FILTER (features from gvainference): "
             "max_iou_distance=%.3f, max_age=%d, n_init=%d, "
             "max_cosine_distance=%.3f",
             max_iou_distance_, max_age_, n_init_, max_cosine_distance_);
}

/**
 * @brief Main tracking function - process frame detections and update tracks with IDs
 */
void DeepSortTracker::track(dlstreamer::FramePtr buffer, GVA::VideoFrame &frame_meta) {
    if (!buffer) {
        throw std::invalid_argument("DeepSortTracker: buffer is nullptr");
    }

    // Map buffer to system memory for OpenCV access
    dlstreamer::FramePtr sys_buffer = buffer_mapper_->map(buffer, dlstreamer::AccessMode::Read);
    MappedMat mapped_mat(sys_buffer);
    cv::Mat raw_image = mapped_mat.mat();

    // Convert to RGB if needed for feature extraction (Deep SORT requires 3-channel input)
    cv::Mat image;
    do_color_space_conversion(image, raw_image, sys_buffer);

    auto regions = frame_meta.regions();

    // Convert GVA regions to detections (using FeatureExtractor or pre-extracted features)
    std::vector<Detection> detections = convert_detections(image, regions);

    // Predict existing tracks
    for (auto &track : tracks_) {
        track->mark_missed();
    }

    // Associate detections to tracks
    std::vector<std::pair<int, int>> matches;
    std::vector<int> unmatched_dets, unmatched_trks;
    associate_detections_to_tracks(detections, matches, unmatched_dets, unmatched_trks);

    //  Update matched tracks and assign object IDs to existing regions
    for (const auto &match : matches) {
        tracks_[match.second]->update(detections[match.first]);

        auto &detection = detections[match.first];
        auto &track = tracks_[match.second];
        cv::Rect_<float> track_bbox = track->to_bbox();
        GST_DEBUG("{%s} Updating matched tracks: det-bbox[%d][%.1f,%.1f,%.1fx%.1f], trk-bbox[%d][%.1f,%.1f,%.1fx%.1f], "
                  "track_id=%d, track_state=%s",
                  __FUNCTION__, match.first, detection.bbox.x, detection.bbox.y, detection.bbox.width,
                  detection.bbox.height, match.second, track_bbox.x, track_bbox.y, track_bbox.width, track_bbox.height,
                  track->track_id(), track->state_str().c_str());

        // Assign tracking ID to the existing region only if track is confirmed
        // This follows Deep SORT convention where only confirmed tracks get persistent IDs
        if (match.first < static_cast<int>(regions.size()) && tracks_[match.second]->is_confirmed()) {
            regions[match.first].set_object_id(tracks_[match.second]->track_id());
        }
    }

    // Create new tracks for unmatched detections
    for (int det_idx : unmatched_dets) {
        auto new_track = std::make_unique<Track>(detections[det_idx].bbox, next_id_++, n_init_, max_age_,
                                                 detections[det_idx].feature);
        int new_track_id = new_track->track_id();
        std::string track_state = new_track->state_str();
        GST_DEBUG("{%s} New track created: ID=%d, bbox[%.1f, %.1f, %.1f x %.1f], state=%s", __FUNCTION__, new_track_id,
                  detections[det_idx].bbox.x, detections[det_idx].bbox.y, detections[det_idx].bbox.width,
                  detections[det_idx].bbox.height, track_state.c_str());
        tracks_.push_back(std::move(new_track));
    }

    // Remove deleted tracks
    tracks_.erase(std::remove_if(tracks_.begin(), tracks_.end(),
                                 [](const std::unique_ptr<Track> &track) { return track->is_deleted(); }),
                  tracks_.end());
}

void DeepSortTracker::do_color_space_conversion(cv::Mat &image, cv::Mat &raw_image, dlstreamer::FramePtr sys_buffer) {
    dlstreamer::ImageFormat format = static_cast<dlstreamer::ImageFormat>(sys_buffer->format());

    // Convert color space based on input format
    switch (format) {
    case dlstreamer::ImageFormat::BGR:
        // Already in correct format, just clone to avoid modifying original
        image = raw_image.clone();
        break;
    case dlstreamer::ImageFormat::NV12:
        cv::cvtColor(raw_image, image, cv::COLOR_YUV2BGR_NV12);
        break;
    case dlstreamer::ImageFormat::I420:
        cv::cvtColor(raw_image, image, cv::COLOR_YUV2BGR_I420);
        break;
    case dlstreamer::ImageFormat::BGRX:
        cv::cvtColor(raw_image, image, cv::COLOR_BGRA2BGR);
        break;
    case dlstreamer::ImageFormat::RGBX:
        cv::cvtColor(raw_image, image, cv::COLOR_RGBA2BGR);
        break;
    default:
        GST_ERROR("Unsupported video format %d for Deep SORT feature extraction", static_cast<int>(format));
        // Fallback: try to use as-is if it has 3 channels
        if (raw_image.channels() == 3) {
            image = raw_image.clone();
        } else {
            GST_ERROR("Cannot convert %d-channel image to RGB", raw_image.channels());
            return; // Early return on unsupported format
        }
        break;
    }
}

/**
 * @brief Convert GVA region detections to Deep SORT Detection objects
 */
std::vector<Detection> DeepSortTracker::convert_detections(const cv::Mat &image,
                                                           const std::vector<GVA::RegionOfInterest> &regions) {
    std::vector<Detection> detections;
    detections.reserve(regions.size());

    if (feature_extractor_) {
        //  Mode 1: Use internal FeatureExtractor for feature extraction
        std::vector<cv::Rect> bboxes;
        for (const auto &region : regions) {
            cv::Rect bbox(region.rect().x, region.rect().y, region.rect().w, region.rect().h);
            bboxes.push_back(bbox);
        }

        auto features = feature_extractor_->extract_batch(image, bboxes);

        for (size_t i = 0; i < regions.size(); ++i) {
            const auto &region = regions[i];
            cv::Rect_<float> bbox(region.rect().x, region.rect().y, region.rect().w, region.rect().h);
            float confidence = region.confidence();

            GST_DEBUG("{%s} Detection %zu (FeatureExtractor): bbox[%d,%d,%d,%d], confidence=%.3f, feature_size=%zu",
                      __FUNCTION__, i, (int)bbox.x, (int)bbox.y, (int)bbox.width, (int)bbox.height, confidence,
                      features[i].size());

            detections.emplace_back(bbox, confidence, features[i], -1);
        }
    } else {
        // Mode 2: Extract features from pre-attached tensor data (from gvainference)
        for (size_t i = 0; i < regions.size(); ++i) {
            const auto &region = regions[i];
            cv::Rect_<float> bbox(region.rect().x, region.rect().y, region.rect().w, region.rect().h);
            float confidence = region.confidence();

            GST_DEBUG("Processing region %zu: bbox[x=%d,y=%d,w=%d,h=%d], confidence=%.3f", i, (int)bbox.x, (int)bbox.y,
                      (int)bbox.width, (int)bbox.height, confidence);

            // Extract feature vector from tensor data attached to the region (from gvainference)
            std::vector<float> feature_vector;
            bool found_feature = false;

            // Look for feature tensor in region's tensors (added by gvainference element)
            auto tensors = region.tensors();
            for (const auto &tensor : tensors) {
                // Look for feature/embedding tensor from gvainference
                std::string tensor_name = tensor.name();
                std::string layer_name = tensor.layer_name();

                // Check if this is a feature tensor (common layer names for feature extraction)
                if (layer_name.find("output") != std::string::npos &&
                    tensor_name.find("inference_layer_name:output") != std::string::npos) {

                    // Extract feature data from tensor
                    feature_vector = tensor.data<float>();

                    if (!feature_vector.empty() && feature_vector.size() == DEFAULT_FEATURES_VECTOR_SIZE_128) {
                        // L2 normalize the feature vector (standard for Deep SORT)
                        float norm = std::sqrt(std::inner_product(feature_vector.begin(), feature_vector.end(),
                                                                  feature_vector.begin(), 0.0f));
                        if (norm > 0.0f) {
                            for (float &f : feature_vector) {
                                f /= norm;
                            }
                        }
                        found_feature = true;
                        break;
                    }
                }
            }

            // If no feature found, use zero vector (will disable appearance-based matching)
            if (!found_feature) {
                GST_WARNING("No feature tensor found for region %zu, using zero feature (motion-only tracking)", i);
                feature_vector =
                    std::vector<float>(DEFAULT_FEATURES_VECTOR_SIZE_128, 0.0f); // Default 128-dimensional zero vector
            }

            GST_DEBUG("{%s} Detection %zu (gvainference): bbox[%d,%d,%d,%d], confidence=%.3f, feature_size=%zu",
                      __FUNCTION__, i, (int)bbox.x, (int)bbox.y, (int)bbox.width, (int)bbox.height, confidence,
                      feature_vector.size());

            detections.emplace_back(bbox, confidence, feature_vector, -1);
        }
    }

    return detections;
}

/**
 * @brief Associate current detections with existing tracks using IoU and feature distance
 */
void DeepSortTracker::associate_detections_to_tracks(const std::vector<Detection> &detections,
                                                     std::vector<std::pair<int, int>> &matches,
                                                     std::vector<int> &unmatched_dets,
                                                     std::vector<int> &unmatched_trks) {
    matches.clear();
    unmatched_dets.clear();
    unmatched_trks.clear();

    if (tracks_.empty()) {
        for (size_t i = 0; i < detections.size(); ++i) {
            unmatched_dets.push_back(i);
        }
        return;
    }

    // Build cost matrix combining IoU and cosine distance
    std::vector<std::vector<float>> cost_matrix(detections.size(), std::vector<float>(tracks_.size(), 1.0f));

    for (size_t det_idx = 0; det_idx < detections.size(); ++det_idx) {
        for (size_t trk_idx = 0; trk_idx < tracks_.size(); ++trk_idx) {

            cv::Rect_<float> track_bbox = tracks_[trk_idx]->to_bbox();
            float iou = calculate_iou(detections[det_idx].bbox, track_bbox);

            GST_DEBUG("{%s} Detection vs Track : det_bbox[%zu][%.1f, %.1f, %.1f, %.1f] vs track_bbox[%zu][%.1f, "
                      "%.1f, %.1f, "
                      "%.1f] ; iou=%.3f",
                      __FUNCTION__, det_idx, detections[det_idx].bbox.x, detections[det_idx].bbox.y,
                      detections[det_idx].bbox.width, detections[det_idx].bbox.height, trk_idx, track_bbox.x,
                      track_bbox.y, track_bbox.width, track_bbox.height, iou);

            // Reject matches with IoU below threshold (poor overlap)
            if (iou < max_iou_distance_) {
                cost_matrix[det_idx][trk_idx] = 1.0f; // No match
                continue;
            }

            // Calculate minimum cosine distance to track features
            float min_cosine_dist = 1.0f;
            const auto &track_features = tracks_[trk_idx]->features();

            for (const auto &track_feature : track_features) {
                float cosine_dist = calculate_cosine_distance(detections[det_idx].feature, track_feature);
                min_cosine_dist = std::min(min_cosine_dist, cosine_dist);
            }

            if (min_cosine_dist > max_cosine_distance_) {
                cost_matrix[det_idx][trk_idx] = 1.0f; // No match
            } else {
                // Combine IoU and cosine distance
                cost_matrix[det_idx][trk_idx] = 0.5f * (1.0f - iou) + 0.5f * min_cosine_dist;
            }
        }
    }

    // Use Hungarian only when confident about assignments
#if 0    
    if (max_iou > threshold && feature_confidence > threshold) {
        hungarian_assignment(cost_matrix, matches);
    } else {
        // Hungarian assignment (using greedy version for better ID consistency)
        hungarian_assignment_greedy(cost_matrix, matches);
    }
#endif

    // Hungarian assignment (using greedy version for better ID consistency)
    hungarian_assignment_greedy(cost_matrix, matches);

    // Find unmatched detections and tracks
    std::vector<bool> matched_dets(detections.size(), false);
    std::vector<bool> matched_trks(tracks_.size(), false);

    for (const auto &match : matches) {
        matched_dets[match.first] = true;
        matched_trks[match.second] = true;
    }

    for (size_t i = 0; i < detections.size(); ++i) {
        if (!matched_dets[i]) {
            unmatched_dets.push_back(i);
        }
    }

    for (size_t i = 0; i < tracks_.size(); ++i) {
        if (!matched_trks[i]) {
            unmatched_trks.push_back(i);
        }
    }
}

/**
 * @brief Calculate cosine distance between two feature vectors (0=identical, 1=opposite)
 */
float DeepSortTracker::calculate_cosine_distance(const std::vector<float> &feat1, const std::vector<float> &feat2) {
    if (feat1.size() != feat2.size()) {
        return 1.0f; // Maximum distance for mismatched features
    }

    float dot_product = std::inner_product(feat1.begin(), feat1.end(), feat2.begin(), 0.0f);
    return 1.0f - dot_product; // Convert cosine similarity to distance
}

/**
 * @brief Calculate Intersection over Union (IoU) between two bounding boxes (0=no overlap, 1=perfect match)
 */
float DeepSortTracker::calculate_iou(const cv::Rect_<float> &bbox1, const cv::Rect_<float> &bbox2) {
    cv::Rect_<float> intersection_rect = bbox1 & bbox2;
    float intersection_area = intersection_rect.area();
    float union_area = bbox1.area() + bbox2.area() - intersection_area;

    float iou = union_area > 0.0f ? intersection_area / union_area : 0.0f;

    // Debug: Print detailed IoU calculation
    GST_LOG("{%s} IoU calculation: bbox1[%.1f,%.1f,%.1fx%.1f] area=%.1f, bbox2[%.1f,%.1f,%.1fx%.1f] area=%.1f, "
            "intersection[%.1f,%.1f,%.1fx%.1f] area=%.1f, union=%.1f, iou=%.3f",
            __FUNCTION__, bbox1.x, bbox1.y, bbox1.width, bbox1.height, bbox1.area(), bbox2.x, bbox2.y, bbox2.width,
            bbox2.height, bbox2.area(), intersection_rect.x, intersection_rect.y, intersection_rect.width,
            intersection_rect.height, intersection_area, union_area, iou);

    return iou;
}

/**
 * @brief Full Hungarian algorithm implementation for optimal assignment problem
 * @details Kuhn-Munkres algorithm that finds minimum cost perfect matching
 */
void DeepSortTracker::hungarian_assignment(const std::vector<std::vector<float>> &cost_matrix,
                                           std::vector<std::pair<int, int>> &assignments) {
    assignments.clear();

    if (cost_matrix.empty())
        return;

    size_t rows = cost_matrix.size();
    size_t cols = cost_matrix[0].size();

    // Create working matrix (copy of cost matrix)
    std::vector<std::vector<float>> matrix(rows, std::vector<float>(cols));
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            matrix[i][j] = cost_matrix[i][j];
        }
    }

    // Step 1: Subtract row minimums
    for (size_t i = 0; i < rows; ++i) {
        float row_min = *std::min_element(matrix[i].begin(), matrix[i].end());
        for (size_t j = 0; j < cols; ++j) {
            matrix[i][j] -= row_min;
        }
    }

    // Step 2: Subtract column minimums
    for (size_t j = 0; j < cols; ++j) {
        float col_min = matrix[0][j];
        for (size_t i = 1; i < rows; ++i) {
            col_min = std::min(col_min, matrix[i][j]);
        }
        for (size_t i = 0; i < rows; ++i) {
            matrix[i][j] -= col_min;
        }
    }

    // Track assignments and coverage
    std::vector<std::vector<int>> marks(rows, std::vector<int>(cols, 0)); // 0=none, 1=star, 2=prime
    std::vector<bool> row_covered(rows, false);
    std::vector<bool> col_covered(cols, false);

    // Step 3: Cover all zeros with minimum number of lines
    // First, find a zero and star it if no other star in same row/column
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            if (matrix[i][j] == 0.0f && !row_covered[i] && !col_covered[j]) {
                marks[i][j] = 1; // Star this zero
                row_covered[i] = true;
                col_covered[j] = true;
            }
        }
    }

    // Reset coverage for next steps
    std::fill(row_covered.begin(), row_covered.end(), false);
    std::fill(col_covered.begin(), col_covered.end(), false);

    // Cover all columns with starred zeros
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            if (marks[i][j] == 1) {
                col_covered[j] = true;
            }
        }
    }

    // Main Hungarian algorithm loop
    bool done = false;
    while (!done) {
        // Check if we have enough lines to cover all zeros
        size_t covered_cols = 0;
        for (size_t j = 0; j < cols; ++j) {
            if (col_covered[j])
                covered_cols++;
        }

        if (covered_cols >= std::min(rows, cols)) {
            done = true;
        } else {
            // Find an uncovered zero and prime it
            bool found_uncovered_zero = false;
            size_t zero_row = 0, zero_col = 0;

            for (size_t i = 0; i < rows && !found_uncovered_zero; ++i) {
                for (size_t j = 0; j < cols && !found_uncovered_zero; ++j) {
                    if (matrix[i][j] == 0.0f && !row_covered[i] && !col_covered[j]) {
                        zero_row = i;
                        zero_col = j;
                        found_uncovered_zero = true;
                        marks[i][j] = 2; // Prime this zero
                    }
                }
            }

            if (found_uncovered_zero) {
                // Check if there's a starred zero in the same row
                bool star_in_row = false;
                size_t star_col = 0;
                for (size_t j = 0; j < cols; ++j) {
                    if (marks[zero_row][j] == 1) {
                        star_in_row = true;
                        star_col = j;
                        break;
                    }
                }

                if (star_in_row) {
                    // Cover this row and uncover the starred column
                    row_covered[zero_row] = true;
                    col_covered[star_col] = false;
                } else {
                    // Construct alternating path and adjust stars
                    std::vector<std::pair<size_t, size_t>> path;
                    path.push_back({zero_row, zero_col});

                    bool path_done = false;
                    while (!path_done) {
                        // Find starred zero in primed zero's column
                        bool found_star = false;
                        size_t star_row = 0;
                        for (size_t i = 0; i < rows; ++i) {
                            if (marks[i][path.back().second] == 1) {
                                star_row = i;
                                found_star = true;
                                break;
                            }
                        }

                        if (found_star) {
                            path.push_back({star_row, path.back().second});

                            // Find primed zero in starred zero's row
                            for (size_t j = 0; j < cols; ++j) {
                                if (marks[star_row][j] == 2) {
                                    path.push_back({star_row, j});
                                    break;
                                }
                            }
                        } else {
                            path_done = true;
                        }
                    }

                    // Unstar each starred zero and star each primed zero in path
                    for (size_t p = 0; p < path.size(); ++p) {
                        if (p % 2 == 0) {
                            marks[path[p].first][path[p].second] = 1; // Star
                        } else {
                            marks[path[p].first][path[p].second] = 0; // Unstar
                        }
                    }

                    // Clear all primes and reset coverage
                    for (size_t i = 0; i < rows; ++i) {
                        for (size_t j = 0; j < cols; ++j) {
                            if (marks[i][j] == 2)
                                marks[i][j] = 0;
                        }
                    }
                    std::fill(row_covered.begin(), row_covered.end(), false);
                    std::fill(col_covered.begin(), col_covered.end(), false);

                    // Cover columns with starred zeros
                    for (size_t i = 0; i < rows; ++i) {
                        for (size_t j = 0; j < cols; ++j) {
                            if (marks[i][j] == 1) {
                                col_covered[j] = true;
                            }
                        }
                    }
                }
            } else {
                // No uncovered zeros - add minimum uncovered value
                float min_uncovered = std::numeric_limits<float>::max();
                for (size_t i = 0; i < rows; ++i) {
                    for (size_t j = 0; j < cols; ++j) {
                        if (!row_covered[i] && !col_covered[j]) {
                            min_uncovered = std::min(min_uncovered, matrix[i][j]);
                        }
                    }
                }

                // Subtract from uncovered, add to doubly covered
                for (size_t i = 0; i < rows; ++i) {
                    for (size_t j = 0; j < cols; ++j) {
                        if (row_covered[i] && col_covered[j]) {
                            matrix[i][j] += min_uncovered;
                        } else if (!row_covered[i] && !col_covered[j]) {
                            matrix[i][j] -= min_uncovered;
                        }
                    }
                }
            }
        }
    }

    // Extract final assignments from starred positions
    for (size_t i = 0; i < rows; ++i) {
        for (size_t j = 0; j < cols; ++j) {
            if (marks[i][j] == 1) {
                // Apply cost threshold check like the greedy version
                if (cost_matrix[i][j] < 0.5f) {
                    assignments.emplace_back(i, j);
                }
            }
        }
    }
}

/**
 * @brief Simple greedy assignment algorithm
 */
void DeepSortTracker::hungarian_assignment_greedy(const std::vector<std::vector<float>> &cost_matrix,
                                                  std::vector<std::pair<int, int>> &assignments) {
    assignments.clear();

    std::vector<bool> det_assigned(cost_matrix.size(), false);
    std::vector<bool> trk_assigned(cost_matrix.empty() ? 0 : cost_matrix[0].size(), false);

    for (size_t det_idx = 0; det_idx < cost_matrix.size(); ++det_idx) {
        if (det_assigned[det_idx])
            continue;

        float min_cost = 1.0f;
        int best_trk = -1;

        for (size_t trk_idx = 0; trk_idx < cost_matrix[det_idx].size(); ++trk_idx) {
            if (trk_assigned[trk_idx])
                continue;

            if (cost_matrix[det_idx][trk_idx] < min_cost) {
                min_cost = cost_matrix[det_idx][trk_idx];
                best_trk = trk_idx;
            }
        }

        if (best_trk >= 0 && min_cost < 0.5f) { // Threshold for assignment
            assignments.emplace_back(det_idx, best_trk);
            det_assigned[det_idx] = true;
            trk_assigned[best_trk] = true;
        }
    }
}

} // namespace DeepSortWrapper