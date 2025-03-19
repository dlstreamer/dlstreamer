/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_TRACKER_H__
#define __OT_TRACKER_H__

#include "dlstreamer/utils.h"
#include <cstdint>
#include <deque>
#include <opencv2/opencv.hpp>

#include "objects_associator.h"
#include "tracklet.h"

namespace vas {
namespace ot {

const float kMinRegionRatioInImageBoundary = 0.75f; // MIN_REGION_RATIO_IN_IMAGE_BOUNDARY

class Tracker {
  public:
    class InitParameters {
      public:
        bool generate_objects; // short-term if true, zero-term if false
        bool tracking_per_class;

        float kRgbHistDistScale;
        float kNormCenterDistScale;
        float kNormShapeDistScale;

        // Won't be exposed to the external
        float min_region_ratio_in_boundary; // For ST, ZT
    };

  public:
    virtual ~Tracker();

    /**
     * create new object tracker instance
     * @param InitParameters
     */
    static Tracker *CreateInstance(InitParameters init_parameters);

    /**
     * perform tracking
     *
     * @param[in] frame_size Input frame size
     * @param[in] detection Newly detected object data vector which will be added to the tracker. put zero length vector
     *            if there is no new object in the frame.
     * @param[in] delta_t Time passed after the latest call to TrackObjects() in seconds. Use 1.0/FPS in case of
     * constant frame rate
     * @param[out] tracklets Tracked object data vector.
     * @return 0 for success. negative value for failure
     */
    virtual int32_t TrackObjects(cv::Size frame_size, const std::vector<Detection> &detections,
                                 std::vector<std::shared_ptr<Tracklet>> *tracklets, float delta_t = 0.033f);

    /**
     * remove object
     *
     * @param[in] id Object id for removing. it should be the 'id' value of the Tracklet
     * @return 0 for success. negative value for failure.
     */
    int32_t RemoveObject(const int32_t id);

    /**
     * reset all internal state to its initial.
     *
     * @return 0 for success. negative value for failure.
     */
    void Reset(void);

    /**
     * get cumulated frame number
     *
     * @return 0
     */
    int32_t GetFrameCount(void) const;

  protected:
    explicit Tracker(int32_t max_objects, float min_region_ratio_in_boundary, bool class_per_class = true);
    Tracker() = delete;

    int32_t GetNextTrackingID();
    void IncreaseFrameCount();

    void ComputeOcclusion();

    void RemoveOutOfBoundTracklets(int32_t input_width, int32_t input_height, bool is_filtered = false);
    void RemoveDeadTracklets();
    bool RemoveOneLostTracklet();

  protected:
    int32_t next_id_;
    int32_t frame_count_;

    float min_region_ratio_in_boundary_;

    ObjectsAssociator associator_;
    std::vector<std::shared_ptr<Tracklet>> tracklets_;

  public:
    explicit Tracker(vas::ot::Tracker::InitParameters init_param);

    Tracker(const Tracker &) = delete;
    Tracker &operator=(const Tracker &) = delete;

  private:
    void TrimTrajectories();

  private:
    bool generate_objects;
    cv::Size image_sz;

    int32_t kMaxAssociationLostCount = 2;    // ST_TRACKED -> ST_LOST
    int32_t kMaxAssociationFailCount = 120;  // ST_LOST -> ST_DEAD, 120 = ~4 seconds
    int32_t kMaxOutdatedCountInTracked = 30; // ST_TRACKED -> ST_LOST
    int32_t kMaxOutdatedCountInLost = 20;    // ST_LOST -> ST_DEAD
    size_t kMaxTrajectorySize = 30;
    size_t kMaxRgbFeatureHistory = 1;
    int32_t kMinBirthCount = 3;
    float kMaxOcclusionRatioForModelUpdate = 0.4f;
};

}; // namespace ot
}; // namespace vas

#endif // __OT_TRACKER_H__
