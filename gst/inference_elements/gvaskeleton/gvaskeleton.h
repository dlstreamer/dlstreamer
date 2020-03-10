#pragma once

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef __cplusplus
namespace human_pose_estimation {

class HumanPoseEstimator;

} // namespace human_pose_estimation

using HumanPoseEstimator = human_pose_estimation::HumanPoseEstimator;
#else  /* __cplusplus */
typedef struct HumanPoseEstimator HumanPoseEstimator;
#endif /* __cplusplus */

typedef enum { GVA_SKELETON_OK, GVA_SKELETON_ERROR } GvaSkeletonStatus;

HumanPoseEstimator *hpe_initialization(char *, char *);
GvaSkeletonStatus hpe_release(HumanPoseEstimator *);
GvaSkeletonStatus hpe_to_estimate(HumanPoseEstimator *, GstBuffer *, gboolean, GstVideoInfo *);

#ifdef __cplusplus
}
#endif /* __cplusplus */
