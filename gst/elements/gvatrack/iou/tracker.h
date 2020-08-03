/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 * Reidentification gallery implementation based on smart classroom demo
 * See https://github.com/opencv/open_model_zoo/tree/2018/demos/smart_classroom_demo
 * Differences:
 * Store features in separate feature file instead of embedding into images
 * Adapted code style to match with Video Analytics GStreamer* plugins project
 * Fixed warnings
 ******************************************************************************/

#pragma once

#include "itracker.h"
#include "tracked_objects.h"
#include "video_frame.h"
#include <gst/video/video.h>

#include <functional>
#include <memory>
#include <set>
#include <unordered_map>

namespace iou {

///
/// \brief The Params struct stores parameters of Tracker.
///
struct TrackerParams {
    size_t min_track_duration; ///< Min track duration in frames

    size_t forget_delay; ///< Forget about track if the last bounding box in
    /// track was detected more than specified number of
    /// frames ago.

    float affinity_thr; ///< Affinity threshold which is used to determine if
    /// tracklet and detection should be combined.

    float shape_affinity_w; ///< Shape affinity weight.

    float motion_affinity_w; ///< Motion affinity weight.

    float min_det_conf; ///< Min confidence of detection.

    int averaging_window_size; ///< The number of objects in track for averaging predictions.

    cv::Vec2f bbox_aspect_ratios_range; ///< Bounding box aspect ratios range.

    cv::Vec2f bbox_heights_range; ///< Bounding box heights range.

    bool drop_forgotten_tracks; ///< Drop forgotten tracks. If it's enabled it
    /// disables an ability to get detection log.

    int max_num_objects_in_track; ///< The number of objects in track is
    /// restricted by this parameter. If it is negative or zero, the max number of
    /// objects in track is not restricted.

    std::string objects_type; ///< The type of boxes which will be grabbed from
    /// detector. Boxes with other types are ignored.

    ///
    /// Default constructor.
    ///
    TrackerParams();
};

///
/// \brief Simple Hungarian algorithm-based tracker.
///
class Tracker : public ITracker {
  public:
    ///
    /// \brief Constructor that creates an instance of Tracker with
    /// parameters.
    /// \param[in] params Tracker parameters.
    ///
    explicit Tracker(const GstVideoInfo *video_info, const TrackerParams &params = TrackerParams());

    ~Tracker() = default;
    static ITracker *Create(const GstVideoInfo *video_info);

    ///
    /// \brief Pipeline parameters getter.
    /// \return Parameters of pipeline.
    ///
    const TrackerParams &params() const;

    ///
    /// \brief Pipeline parameters setter.
    /// \param[in] params Parameters of pipeline.
    ///
    void set_params(const TrackerParams &params);

    ///
    /// \brief Reset the pipeline.
    ///
    void Reset();

    ///
    /// \brief Returns number of counted tracks.
    /// \return a number of counted tracks.
    ///
    size_t Count() const;

    ///
    /// \brief Returns recently detected objects.
    /// \return recently detected objects.
    ///
    const TrackedObjects &detections() const;

    ///
    /// \brief Get active tracks to draw
    /// \return Active tracks.
    ///
    std::unordered_map<size_t, std::vector<cv::Point>> GetActiveTracks() const;

    ///
    /// \brief Get tracked detections.
    /// \return Tracked detections.
    ///
    TrackedObjects TrackedDetections() const;

    ///
    /// \brief Get tracked detections with labels.
    /// \return Tracked detections.
    ///
    TrackedObjects TrackedDetectionsWithLabels() const;

    ///
    /// \brief IsTrackForgotten returns true if track is forgotten.
    /// \param id Track ID.
    /// \return true if track is forgotten.
    ///
    bool IsTrackForgotten(size_t id) const;

    ///
    /// \brief tracks Returns all tracks including forgotten (lost too many frames
    /// ago).
    /// \return Set of tracks {id, track}.
    ///
    const std::unordered_map<size_t, Track> &tracks() const;

    ///
    /// \brief tracks Returns all tracks including forgotten (lost too many frames
    /// ago).
    /// \return Vector of tracks
    ///
    std::vector<Track> vector_tracks() const;

    ///
    /// \brief IsTrackValid Checks whether track is valid (duration > threshold).
    /// \param id Index of checked track.
    /// \return True if track duration exceeds some predefined value.
    ///
    bool IsTrackValid(size_t id) const;

    ///
    /// \brief DropForgottenTracks Removes tracks from memory that were lost too
    /// many frames ago.
    ///
    void DropForgottenTracks();

    ///
    /// \brief Process given frame.
    /// \param buffer Input gst buffer
    ///
    void track(GstBuffer *buffer) override;

  private:
    void Process();
    void DropForgottenTrack(size_t track_id);

    const std::set<size_t> &active_track_ids() const {
        return active_track_ids_;
    }

    float ShapeAffinity(const cv::Rect &trk, const cv::Rect &det);
    float MotionAffinity(const cv::Rect &trk, const cv::Rect &det);

    void SolveAssignmentProblem(const std::set<size_t> &track_ids, const TrackedObjects &detections,
                                std::set<size_t> *unmatched_tracks, std::set<size_t> *unmatched_detections,
                                std::set<std::tuple<size_t, size_t, float>> *matches);
    void FilterDetectionsAndStore(GVA::VideoFrame &roi_list);

    void ComputeDissimilarityMatrix(const std::set<size_t> &active_track_ids, const TrackedObjects &detections,
                                    cv::Mat *dissimilarity_matrix);

    std::vector<std::pair<size_t, size_t>>
    GetTrackToDetectionIds(const std::set<std::tuple<size_t, size_t, float>> &matches);

    float Distance(const TrackedObject &obj1, const TrackedObject &obj2);

    void AddNewTrack(const TrackedObject &detection);

    void AddNewTracks(const TrackedObjects &detections);

    void AddNewTracks(const TrackedObjects &detections, const std::set<size_t> &ids);

    void AppendToTrack(size_t track_id, const TrackedObject &detection);

    bool EraseTrackIfBBoxIsOutOfFrame(size_t track_id);

    bool EraseTrackIfItWasLostTooManyFramesAgo(size_t track_id);

    bool UptateLostTrackAndEraseIfItsNeeded(size_t track_id);

    void UpdateLostTracks(const std::set<size_t> &track_ids);

    std::unordered_map<size_t, std::vector<cv::Point>> GetActiveTracks();

    // Parameters of the pipeline.
    TrackerParams params_;

    // Indexes of active tracks.
    std::set<size_t> active_track_ids_;

    // All tracks.
    std::unordered_map<size_t, Track> tracks_;

    // Recent detections.
    TrackedObjects detections_;

    // Number of all current tracks.
    size_t tracks_counter_;

    // Number of dropped valid tracks.
    size_t valid_tracks_counter_;

    cv::Size frame_size_;
    size_t frame_number;

    std::unique_ptr<GstVideoInfo, std::function<void(GstVideoInfo *)>> video_info;
};

int LabelWithMaxFrequencyInTrack(const Track &track);
std::vector<Track> UpdateTrackLabelsToBestAndFilterOutUnknowns(const std::vector<Track> &tracks);

} // namespace iou
