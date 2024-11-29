/*******************************************************************************
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifndef __GST_ANALYTICS_KEYPOINTS_MTD__
#define __GST_ANALYTICS_KEYPOINTS_MTD__

#include <gst/analytics/analytics-meta-prelude.h>
#include <gst/analytics/gstanalyticsmeta.h>
#include <gst/gst.h>

G_BEGIN_DECLS
/**
 * GstAnalyticsKeypointsMtd:
 * @id: Instance identifier.
 * @meta: Instance of #GstAnalyticsRelationMeta where the analysis-metadata
 * identified by @id is stored.
 *
 * Handle containing data required to use gst_analytics_keypoints_mtd APIs.
 * This type is generally expected to be allocated on the stack.
 *
 * Since: 1.26
 */
typedef struct _GstAnalyticsMtd GstAnalyticsKeypointsMtd;

/**
 * GstAnalyticsKeypointDimensions:
 * @GST_ANALYTICS_KEYPOINT_DIMENSIONS_2D: keypoints in 2D space with (x,y) coordinates.
 * @GST_ANALYTICS_KEYPOINT_DIMENSIONS_3D: keypoints in 3D space with (x,y,z) coordinates.
 *
 * Enum value describing supported keypoint dimension.
 * */
typedef enum {
    GST_ANALYTICS_KEYPOINT_DIMENSIONS_2D = 2,
    GST_ANALYTICS_KEYPOINT_DIMENSIONS_3D = 3,
} GstAnalyticsKeypointDimensions;

/**
 * GstKeypointPair:
 * @kp1: index of the first keypoint in a skeleton link.
 * @kp2: index of the second keypoint in a skeleton link.
 *
 * A pair of keypoints linked in a skeleton.
 * */
typedef struct {
    guint kp1;
    guint kp2;
} GstKeypointPair;

GST_ANALYTICS_META_API
GstAnalyticsMtdType gst_analytics_keypoints_mtd_get_mtd_type(void);

GST_ANALYTICS_META_API
gsize gst_analytics_keypoints_mtd_get_count(const GstAnalyticsKeypointsMtd *handle);

GST_ANALYTICS_META_API
GstAnalyticsKeypointDimensions gst_analytics_keypoints_mtd_get_dimension(const GstAnalyticsKeypointsMtd *handle);

GST_ANALYTICS_META_API
gsize gst_analytics_keypoints_mtd_get_confidence_count(const GstAnalyticsKeypointsMtd *handle);

GST_ANALYTICS_META_API
gsize gst_analytics_keypoints_mtd_get_skeleton_count(const GstAnalyticsKeypointsMtd *handle);

GST_ANALYTICS_META_API
gboolean gst_analytics_keypoints_mtd_get_position(const GstAnalyticsKeypointsMtd *handle, gfloat *position,
                                                  gsize index);

GST_ANALYTICS_META_API
gboolean gst_analytics_keypoints_mtd_get_confidence(const GstAnalyticsKeypointsMtd *handle, gfloat *confidence,
                                                    gsize index);

GST_ANALYTICS_META_API
gboolean gst_analytics_keypoints_mtd_get_skeleton(const GstAnalyticsKeypointsMtd *handle, GstKeypointPair *skeleton,
                                                  gsize index);

GST_ANALYTICS_META_API
gboolean gst_analytics_relation_meta_add_keypoints_mtd(GstAnalyticsRelationMeta *instance, const gsize keypoint_count,
                                                       const GstAnalyticsKeypointDimensions keypoint_dimensions,
                                                       const gfloat *positions, const gfloat *confidences,
                                                       const gsize skeleton_count, const GstKeypointPair *skeletons,
                                                       GstAnalyticsKeypointsMtd *keypoint_mtd);

G_END_DECLS
#endif // __GST_ANALYTICS_KEYPOINTS_MTD__