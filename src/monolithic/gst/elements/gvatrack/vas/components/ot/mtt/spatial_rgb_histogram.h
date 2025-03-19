/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __OT_SPATIAL_RGB_HISTOGRAM_H__
#define __OT_SPATIAL_RGB_HISTOGRAM_H__

#include "vas/components/ot/mtt/rgb_histogram.h"

#include "vas/components/ot/container/yuv_image.h"

namespace vas {
namespace ot {

class SpatialRgbHistogram : public RgbHistogram {
  public:
    SpatialRgbHistogram(int32_t canonical_patch_size, int32_t spatial_bin_size, int32_t spatial_bin_stride,
                        int32_t rgb_bin_size);
    virtual ~SpatialRgbHistogram(void);

    virtual void Compute(const cv::Mat &image, cv::Mat *hist);
    virtual void ComputeFromBgra32(const cv::Mat &image, cv::Mat *hist);
    virtual void ComputeFromNv12(const YuvImage &image, const cv::Rect &roi, cv::Mat *hist);
    virtual void ComputeFromI420(const YuvImage &image, const cv::Rect &roi, cv::Mat *hist);

    virtual int32_t FeatureSize(void) const;

  protected:
    int32_t canonical_patch_size_;
    int32_t spatial_bin_size_;
    int32_t spatial_bin_stride_;
    int32_t spatial_num_bins_;
    int32_t spatial_hist_size_;
    cv::Mat weight_;
};

}; // namespace ot
}; // namespace vas

#endif // __OT_SPATIAL_RGB_HISTOGRAM_H__
