/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_SHORT_TERM_IMAGELESS_TRACKER_H__
#define __OT_SHORT_TERM_IMAGELESS_TRACKER_H__

#include <deque>
#include <vector>

#include "vas/components/ot/tracker.h"

namespace vas {
namespace ot {

class ShortTermImagelessTracker : public Tracker {
  public:
    explicit ShortTermImagelessTracker(vas::ot::Tracker::InitParameters init_param);
    virtual ~ShortTermImagelessTracker();

    virtual int32_t TrackObjects(const cv::Mat &mat, const std::vector<Detection> &detections,
                                 std::vector<std::shared_ptr<Tracklet>> *tracklets, float delta_t);

    ShortTermImagelessTracker(const ShortTermImagelessTracker &) = delete;
    ShortTermImagelessTracker &operator=(const ShortTermImagelessTracker &) = delete;

  private:
    void TrimTrajectories();

  private:
    cv::Size image_sz;
};

}; // namespace ot
}; // namespace vas

#endif // __OT_SHORT_TERM_IMAGELESS_TRACKER_H__
