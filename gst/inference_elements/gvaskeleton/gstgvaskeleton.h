#pragma once

#ifndef _GST_GVA_SKELETON_H_
#define _GST_GVA_SKELETON_H_

#include <gst/base/gstbasetransform.h>
#include <gst/gst.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GVA_SKELETON (gst_gva_skeleton_get_type())
#define GST_GVA_SKELETON(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_GVA_SKELETON, GstGvaSkeleton))
#define GST_GVA_SKELETON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GVA_SKELETON, GstGvaSkeletonClass))
#define GST_IS_GVA_SKELETON(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_GVA_SKELETON))
#define GST_IS_GVA_SKELETON_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GVA_SKELETON))

#ifdef __cplusplus
namespace human_pose_estimation {
class HumanPoseEstimator;
}
#else  /* __cplusplus */
typedef struct HumanPoseEstimator HumanPoseEstimator;
#endif /* __cplusplus */

typedef struct _GstGvaSkeleton {
    GstBaseTransform element;

    GstVideoInfo info;

    gchar *model_path;
    gchar *device;
    gboolean is_initialized;

    HumanPoseEstimator *hpe_object;

} GstGvaSkeleton;

typedef struct _GstGvaSkeletonClass {
    GstBaseTransformClass parent_class;
} GstGvaSkeletonClass;

GType gst_gva_skeleton_get_type(void);

G_END_DECLS

#endif /* _GST_GVA_SKELETON_H_ */
