/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_ZERO_TERM_CHIST_TRACKER_H__
#define __OT_ZERO_TERM_CHIST_TRACKER_H__

#include <deque>
#include <vector>

#include "vas/components/ot/mtt/spatial_rgb_histogram.h"
#include "vas/components/ot/tracker.h"

namespace vas {
namespace ot {

class ZeroTermChistTracker : public Tracker {
  public:
    explicit ZeroTermChistTracker(vas::ot::Tracker::InitParameters init_param);
    virtual ~ZeroTermChistTracker();

    virtual int32_t TrackObjects(const cv::Mat &mat, const std::vector<Detection> &detections,
                                 std::vector<std::shared_ptr<Tracklet>> *tracklets, float delta_t);

    ZeroTermChistTracker() = delete;
    ZeroTermChistTracker(const ZeroTermChistTracker &) = delete;
    ZeroTermChistTracker &operator=(const ZeroTermChistTracker &) = delete;

  private:
    void TrimTrajectories();

  private:
    SpatialRgbHistogram rgb_hist_;
};

}; // namespace ot
}; // namespace vas

#endif // __OT_ZERO_TERM_CHIST_TRACKER_H__
