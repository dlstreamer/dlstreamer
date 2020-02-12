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

#include "tracker.h"
#include "gva_tensor_meta.h"
#include "gva_utils.h"
#include "kuhn_munkres.h"

using namespace iou;

namespace {

inline bool IsInRange(float x, const cv::Vec2f &v) {
    return v[0] <= x && x <= v[1];
}

inline bool IsInRange(float x, float a, float b) {
    return a <= x && x <= b;
}

cv::Point Center(const cv::Rect &rect) {
    return cv::Point(rect.x + rect.width * .5, rect.y + rect.height * .5);
}

} // namespace

TrackerParams::TrackerParams()
    : min_track_duration(1), forget_delay(150), affinity_thr(0.8), shape_affinity_w(0.5), motion_affinity_w(0.2),
      min_det_conf(0.0), averaging_window_size(1), bbox_aspect_ratios_range(0.666, 5.0), bbox_heights_range(10, 1080),
      drop_forgotten_tracks(false), max_num_objects_in_track(std::numeric_limits<int>::max()), objects_type("face") {
}

Tracker::Tracker(const GstVideoInfo *video_info, const TrackerParams &params)
    : params_(params), tracks_counter_(0), valid_tracks_counter_(0),
      frame_size_(cv::Size(video_info->width, video_info->height)), frame_number(0),
      video_info(gst_video_info_copy(video_info), gst_video_info_free) {
}

ITracker *Tracker::Create(const GstVideoInfo *video_info) {
    return new Tracker(video_info);
}

void Tracker::set_params(const TrackerParams &params) {
    params_ = params;
}

void Tracker::FilterDetectionsAndStore(GVA::VideoFrame &frame) {
    detections_.clear();

    std::vector<TrackedObject> tracked_objects;

    size_t i = 0;
    for (auto &roi : frame.regions()) {
        GstVideoRegionOfInterestMeta *meta = roi.meta();
        cv::Rect rect(meta->x, meta->y, meta->w, meta->h);

        float aspect_ratio = static_cast<float>(rect.height) / rect.width;
        if (roi.confidence() > params_.min_det_conf && IsInRange(aspect_ratio, params_.bbox_aspect_ratios_range) &&
            IsInRange(rect.height, params_.bbox_heights_range)) {
            TrackedObject tracked_obj(rect, roi.confidence(), -1, i, -1);
            for (GVA::Tensor &tensor : roi) {
                if (tensor.has_field("label_id")) {
                    tracked_obj.label = tensor.get_int("label_id");
                    break;
                }
            }

            detections_.emplace_back(tracked_obj);
        }
        ++i;
    }
}

void Tracker::SolveAssignmentProblem(const std::set<size_t> &track_ids, const TrackedObjects &detections,
                                     std::set<size_t> *unmatched_tracks, std::set<size_t> *unmatched_detections,
                                     std::set<std::tuple<size_t, size_t, float>> *matches) {
    CV_Assert(unmatched_tracks);
    CV_Assert(unmatched_detections);
    unmatched_tracks->clear();
    unmatched_detections->clear();

    CV_Assert(!track_ids.empty());
    CV_Assert(!detections.empty());
    CV_Assert(matches);
    matches->clear();

    cv::Mat dissimilarity;
    ComputeDissimilarityMatrix(track_ids, detections, &dissimilarity);

    std::vector<size_t> res = KuhnMunkres().Solve(dissimilarity);

    for (size_t i = 0; i < detections.size(); i++) {
        unmatched_detections->insert(i);
    }

    size_t i = 0;
    for (auto id : track_ids) {
        if (res[i] < detections.size()) {
            matches->emplace(id, res[i], 1 - dissimilarity.at<float>(i, res[i]));
        } else {
            unmatched_tracks->insert(id);
        }
        i++;
    }
}

bool Tracker::EraseTrackIfBBoxIsOutOfFrame(size_t track_id) {
    if (tracks_.find(track_id) == tracks_.end())
        return true;
    auto c = Center(tracks_.at(track_id).back().rect);
    if (frame_size_ != cv::Size() && (c.x < 0 || c.y < 0 || c.x > frame_size_.width || c.y > frame_size_.height)) {
        tracks_.at(track_id).lost = params_.forget_delay + 1;
        active_track_ids_.erase(track_id);
        return true;
    }
    return false;
}

bool Tracker::EraseTrackIfItWasLostTooManyFramesAgo(size_t track_id) {
    if (tracks_.find(track_id) == tracks_.end())
        return true;
    if (tracks_.at(track_id).lost > params_.forget_delay) {
        active_track_ids_.erase(track_id);
        return true;
    }
    return false;
}

bool Tracker::UptateLostTrackAndEraseIfItsNeeded(size_t track_id) {
    tracks_.at(track_id).lost++;
    bool erased = EraseTrackIfBBoxIsOutOfFrame(track_id);
    if (!erased)
        erased = EraseTrackIfItWasLostTooManyFramesAgo(track_id);
    return erased;
}

void Tracker::UpdateLostTracks(const std::set<size_t> &track_ids) {
    for (auto track_id : track_ids) {
        UptateLostTrackAndEraseIfItsNeeded(track_id);
    }
}

void Tracker::Process() {
    CV_Assert(frame_size_ != cv::Size());

    for (auto &obj : detections_) {
        obj.frame_idx = frame_number;
    }
    ++frame_number;

    auto active_tracks = active_track_ids_;

    if (!active_tracks.empty() && !detections_.empty()) {
        std::set<size_t> unmatched_tracks, unmatched_detections;
        std::set<std::tuple<size_t, size_t, float>> matches;

        SolveAssignmentProblem(active_tracks, detections_, &unmatched_tracks, &unmatched_detections, &matches);

        for (const auto &match : matches) {
            size_t track_id = std::get<0>(match);
            size_t det_id = std::get<1>(match);
            float conf = std::get<2>(match);
            if (conf > params_.affinity_thr) {
                AppendToTrack(track_id, detections_[det_id]);
                unmatched_detections.erase(det_id);
            } else {
                unmatched_tracks.insert(track_id);
            }
        }

        AddNewTracks(detections_, unmatched_detections);
        UpdateLostTracks(unmatched_tracks);

        for (size_t id : active_tracks) {
            EraseTrackIfBBoxIsOutOfFrame(id);
        }
    } else {
        AddNewTracks(detections_);
        UpdateLostTracks(active_tracks);
    }

    if (params_.drop_forgotten_tracks)
        DropForgottenTracks();
}

void Tracker::DropForgottenTracks() {
    std::unordered_map<size_t, Track> new_tracks;
    std::set<size_t> new_active_tracks;

    size_t max_id = 0;
    if (!active_track_ids_.empty())
        max_id = *std::max_element(active_track_ids_.begin(), active_track_ids_.end());

    const size_t kMaxTrackID = 10000;
    bool reassign_id = max_id > kMaxTrackID;

    size_t counter = 0;
    for (const auto &pair : tracks_) {
        if (!IsTrackForgotten(pair.first)) {
            new_tracks.emplace(reassign_id ? counter : pair.first, pair.second);
            new_active_tracks.emplace(reassign_id ? counter : pair.first);
            counter++;

        } else {
            if (IsTrackValid(pair.first)) {
                valid_tracks_counter_++;
            }
        }
    }
    tracks_.swap(new_tracks);
    active_track_ids_.swap(new_active_tracks);

    tracks_counter_ = reassign_id ? counter : tracks_counter_;
}

void Tracker::DropForgottenTrack(size_t track_id) {
    CV_Assert(IsTrackForgotten(track_id));
    CV_Assert(active_track_ids_.count(track_id) == 0);
    tracks_.erase(track_id);
}

float Tracker::ShapeAffinity(const cv::Rect &trk, const cv::Rect &det) {
    float w_dist = std::fabs(trk.width - det.width) / (trk.width + det.width);
    float h_dist = std::fabs(trk.height - det.height) / (trk.height + det.height);
    return exp(-params_.shape_affinity_w * (w_dist + h_dist));
}

float Tracker::MotionAffinity(const cv::Rect &trk, const cv::Rect &det) {
    float x_dist = static_cast<float>(trk.x - det.x) * (trk.x - det.x) / (det.width * det.width);
    float y_dist = static_cast<float>(trk.y - det.y) * (trk.y - det.y) / (det.height * det.height);
    return exp(-params_.motion_affinity_w * (x_dist + y_dist));
}

void Tracker::ComputeDissimilarityMatrix(const std::set<size_t> &active_tracks, const TrackedObjects &detections,
                                         cv::Mat *dissimilarity_matrix) {
    dissimilarity_matrix->create(active_tracks.size(), detections.size(), CV_32F);
    size_t i = 0;
    for (auto id : active_tracks) {
        auto ptr = dissimilarity_matrix->ptr<float>(i);
        for (size_t j = 0; j < detections.size(); j++) {
            auto last_det = tracks_.at(id).objects.back();
            ptr[j] = Distance(last_det, detections[j]);
        }
        i++;
    }
}

void Tracker::AddNewTracks(const TrackedObjects &detections) {
    for (size_t i = 0; i < detections.size(); i++) {
        AddNewTrack(detections[i]);
    }
}

void Tracker::AddNewTracks(const TrackedObjects &detections, const std::set<size_t> &ids) {
    for (size_t i : ids) {
        CV_Assert(i < detections.size());
        AddNewTrack(detections[i]);
    }
}

void Tracker::AddNewTrack(const TrackedObject &detection) {
    auto detection_with_id = detection;
    detection_with_id.object_id = tracks_counter_;
    tracks_.emplace(std::pair<size_t, Track>(tracks_counter_, Track({detection_with_id})));

    active_track_ids_.insert(tracks_counter_);
    tracks_counter_++;
}

void Tracker::AppendToTrack(size_t track_id, const TrackedObject &detection) {
    CV_Assert(!IsTrackForgotten(track_id));

    auto detection_with_id = detection;
    detection_with_id.object_id = track_id;

    auto &track = tracks_.at(track_id);

    track.objects.emplace_back(detection_with_id);
    track.lost = 0;
    track.length++;

    if (params_.max_num_objects_in_track > 0) {
        while (track.size() > static_cast<size_t>(params_.max_num_objects_in_track)) {
            track.objects.erase(track.objects.begin());
        }
    }
}

float Tracker::Distance(const TrackedObject &obj1, const TrackedObject &obj2) {
    const float eps = 1e-6;
    float shp_aff = ShapeAffinity(obj1.rect, obj2.rect);
    if (shp_aff < eps)
        return 1.0;

    float mot_aff = MotionAffinity(obj1.rect, obj2.rect);
    if (mot_aff < eps)
        return 1.0;

    return 1.0 - shp_aff * mot_aff;
}

bool Tracker::IsTrackValid(size_t id) const {
    const auto &track = tracks_.at(id);
    const auto &objects = track.objects;
    if (objects.empty()) {
        return false;
    }
    int duration_frames = objects.back().frame_idx - track.first_object.frame_idx;
    if ((size_t)duration_frames < params_.min_track_duration)
        return false;
    return true;
}

bool Tracker::IsTrackForgotten(size_t id) const {
    return tracks_.at(id).lost > params_.forget_delay;
}

void Tracker::Reset() {
    active_track_ids_.clear();
    tracks_.clear();

    detections_.clear();

    tracks_counter_ = 0;
    valid_tracks_counter_ = 0;

    frame_size_ = cv::Size();
}

size_t Tracker::Count() const {
    size_t count = valid_tracks_counter_;
    for (const auto &pair : tracks_) {
        count += (IsTrackValid(pair.first) ? 1 : 0);
    }
    return count;
}

TrackedObjects Tracker::TrackedDetections() const {
    TrackedObjects detections;
    for (size_t idx : active_track_ids()) {
        auto track = tracks_.at(idx);
        if (IsTrackValid(idx) && !track.lost) {
            detections.emplace_back(track.objects.back());
        }
    }
    return detections;
}

TrackedObjects Tracker::TrackedDetectionsWithLabels() const {
    TrackedObjects detections;
    for (size_t idx : active_track_ids()) {
        const auto &track = tracks().at(idx);
        if (IsTrackValid(idx) && !track.lost) {
            TrackedObject object = track.objects.back();
            int counter = 1;
            int start = track.objects.size() >= (size_t)params_.averaging_window_size
                            ? track.objects.size() - params_.averaging_window_size
                            : 0;

            for (size_t i = start; i < track.objects.size() - 1; i++) {
                object.rect.width += track.objects[i].rect.width;
                object.rect.height += track.objects[i].rect.height;
                object.rect.x += track.objects[i].rect.x;
                object.rect.y += track.objects[i].rect.y;
                counter++;
            }
            object.rect.width /= counter;
            object.rect.height /= counter;
            object.rect.x /= counter;
            object.rect.y /= counter;

            object.label = LabelWithMaxFrequencyInTrack(track);

            detections.push_back(object);
        }
    }
    return detections;
}

const std::unordered_map<size_t, Track> &Tracker::tracks() const {
    return tracks_;
}

std::vector<Track> Tracker::vector_tracks() const {
    std::set<size_t> keys;
    for (auto &cur_pair : tracks()) {
        keys.insert(cur_pair.first);
    }
    std::vector<Track> vec_tracks;
    for (size_t k : keys) {
        vec_tracks.push_back(tracks().at(k));
    }
    return vec_tracks;
}

void Tracker::track(GstBuffer *buffer) {
    GVA::VideoFrame frame(buffer, video_info.get());
    FilterDetectionsAndStore(frame);
    Process();

    // Set object id in metadata
    for (TrackedObject &tracked_obj : TrackedDetections())
        if (tracked_obj.object_id >= 0 && tracked_obj.object_index < static_cast<int>(frame.regions().size()))
            set_object_id(frame.regions()[tracked_obj.object_index].meta(), tracked_obj.object_id + 1);
}

int iou::LabelWithMaxFrequencyInTrack(const Track &track) {
    std::unordered_map<int, int> frequencies;
    int max_frequent_count = 0;
    int max_frequent_id = TrackedObject::UNKNOWN_LABEL_IDX;
    for (const auto &detection : track.objects) {
        if (detection.label == TrackedObject::UNKNOWN_LABEL_IDX)
            continue;
        int count = ++frequencies[detection.label];
        if (count > max_frequent_count) {
            max_frequent_count = count;
            max_frequent_id = detection.label;
        }
    }
    return max_frequent_id;
}

std::vector<Track> iou::UpdateTrackLabelsToBestAndFilterOutUnknowns(const std::vector<Track> &tracks) {
    std::vector<Track> new_tracks;
    for (auto &track : tracks) {
        int best_label = LabelWithMaxFrequencyInTrack(track);
        if (best_label == TrackedObject::UNKNOWN_LABEL_IDX)
            continue;

        Track new_track = track;

        for (auto &obj : new_track.objects) {
            obj.label = best_label;
        }
        new_track.first_object.label = best_label;

        new_tracks.emplace_back(std::move(new_track));
    }
    return new_tracks;
}
