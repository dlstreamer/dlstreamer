// Copyright (C) 2018-2019 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//
#include "gstgvaclassify.h"

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#pragma once

G_BEGIN_DECLS

struct HumanPoseEstimator *create_human_pose_estimator(GstGvaClassify *gva_classify);
void release_human_pose_estimator(struct HumanPoseEstimator *gva_humanpose);

G_END_DECLS

#ifdef __cplusplus

#include <string>
#include <vector>

#include <inference_engine.hpp>
#include <opencv2/core/core.hpp>

#include "human_pose.h"

class HumanPoseEstimator {
public:

    HumanPoseEstimator(GstGvaClassify *gva_classify);

    ~HumanPoseEstimator();

    std::vector<HumanPose> postprocess(
        const float* heatMapsData, const int heatMapOffset, const int nHeatMaps,
        const float* pafsData, const int pafOffset, const int nPafs,
        const int featureMapWidth, const int featureMapHeight,
        const cv::Size& imageSize) const ;

private:
    std::vector<HumanPose> extractPoses(const std::vector<cv::Mat>& heatMaps,
                                        const std::vector<cv::Mat>& pafs) const;
    void resizeFeatureMaps(std::vector<cv::Mat>& featureMaps) const;
    void correctCoordinates(std::vector<HumanPose>& poses,
                            const cv::Size& featureMapsSize,
                            const cv::Size& imageSize) const;
    bool inputWidthIsChanged(const cv::Size& imageSize);

    int minJointsNumber;
    int stride;
    cv::Vec4i pad;
    cv::Vec3f meanPixel;
    float minPeaksDistance;
    float midPointsScoreThreshold;
    float foundMidPointsRatioThreshold;
    float minSubsetScore;
    cv::Size inputLayerSize;
    int upsampleRatio;
};
#endif
