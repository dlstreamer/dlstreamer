/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_ZERO_TERM_IMAGELESS_TRACKER_H__
#define __OT_ZERO_TERM_IMAGELESS_TRACKER_H__

#include <deque>
#include <vector>

#include "vas/components/ot/tracker.h"

namespace vas {
namespace ot {

class ZeroTermImagelessTracker : public Tracker {
  public:
    explicit ZeroTermImagelessTracker(vas::ot::Tracker::InitParameters init_param);
    virtual ~ZeroTermImagelessTracker();

    virtual int32_t TrackObjects(const cv::Mat &mat, const std::vector<Detection> &detections,
                                 std::vector<std::shared_ptr<Tracklet>> *tracklets, float delta_t);

    ZeroTermImagelessTracker() = delete;
    ZeroTermImagelessTracker(const ZeroTermImagelessTracker &) = delete;
    ZeroTermImagelessTracker &operator=(const ZeroTermImagelessTracker &) = delete;

  private:
    void TrimTrajectories();
};

}; // namespace ot
}; // namespace vas

#endif // __OT_ZERO_TERM_IMAGELESS_TRACKER_H__
