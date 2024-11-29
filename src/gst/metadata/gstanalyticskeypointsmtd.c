/*******************************************************************************
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "dlstreamer/gst/metadata/gstanalyticskeypointsmtd.h"
#include <string.h>

/**
 * SECTION:gstanalyticskeypointsmtd
 * @title: GstAnalyticsKeypointsMtd
 * @short_description: An analytics metadata for keypoints inside a #GstAnalyticsRelationMeta
 * @symbols:
 * - GstAnalyticsKeypointsMtd
 * @see_also: #GstAnalyticsMtd, #GstAnalyticsRelationMeta
 *
 * This type of metadata holds 2D or 3D keypoints, it is generally used in
 * relationship with another metadata type to enhance its content. For example,
 * it can enhance an object detection medatata from #GstAnalyticsODMtd metadata type.
 *
 * Since: 1.26
 */

static const GstAnalyticsMtdImpl keypoints_impl = {"keypoints", NULL, {NULL}};

typedef struct _GstAnalyticsKeypointsData GstAnalyticsKeypointsData;

struct _GstAnalyticsKeypointsData {
    gsize keypoint_count;
    GstAnalyticsKeypointDimensions keypoint_dimensions;
    gsize confidence_count;
    gsize skeleton_count;
    guint8 data[]; /* must be last, positions + confidence + skeletons */
};

/**
 * gst_analytics_keypoints_mtd_get_mtd_type:
 *
 * Get an id identifying #GstAnalyticsMtd type.
 *
 * Returns: opaque id of #GstAnalyticsMtd type
 *
 * Since: 1.26
 */
GstAnalyticsMtdType gst_analytics_keypoints_mtd_get_mtd_type(void) {
    return (GstAnalyticsMtdType)&keypoints_impl;
}

/**
 * gst_analytics_keypoints_mtd_get_count:
 * @handle: instance handle
 *
 * Get number of keypoints
 * Returns: number of keypoints in this instance of keypoint metadata
 *
 * Since: 1.26
 */
gsize gst_analytics_keypoints_mtd_get_count(const GstAnalyticsKeypointsMtd *handle) {
    g_return_val_if_fail(handle, 0);
    g_return_val_if_fail(handle->meta != NULL, 0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, 0);
    return keypoints_mtd_data->keypoint_count;
}

/**
 * gst_analytics_keypoints_mtd_get_dimension:
 * @handle: instance handle
 *
 * Get keypoint dimension: 2D or 3D
 * Returns: dimension of keypoints in this instance of keypoint metadata
 *
 * Since: 1.26
 */
GstAnalyticsKeypointDimensions gst_analytics_keypoints_mtd_get_dimension(const GstAnalyticsKeypointsMtd *handle) {
    g_return_val_if_fail(handle, 0);
    g_return_val_if_fail(handle->meta != NULL, 0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, 0);
    return keypoints_mtd_data->keypoint_dimensions;
}

/**
 * gst_analytics_keypoints_mtd_get_confidence_count:
 * @handle: instance handle
 *
 * Get number of confidence scores
 * Returns: number of confidence scores for keypoints
 *
 * Since: 1.26
 */
gsize gst_analytics_keypoints_mtd_get_confidence_count(const GstAnalyticsKeypointsMtd *handle) {
    g_return_val_if_fail(handle, 0);
    g_return_val_if_fail(handle->meta != NULL, 0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, 0);
    return keypoints_mtd_data->confidence_count;
}

/**
 * gst_analytics_keypoints_mtd_get_skeleton_count:
 * @handle: instance handle
 *
 * Get number of skeleton links
 * Returns: number of skeleton links in this instance of keypoint metadata
 *
 * Since: 1.26
 */
gsize gst_analytics_keypoints_mtd_get_skeleton_count(const GstAnalyticsKeypointsMtd *handle) {
    g_return_val_if_fail(handle, 0);
    g_return_val_if_fail(handle->meta != NULL, 0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, 0);
    return keypoints_mtd_data->skeleton_count;
}

/**
 * gst_analytics_keypoints_mtd_get_position:
 * @handle: instance handle
 * @position: (array size: keypoint_dimension): xy or xyz coordinates of a keypoint
 * @index: keypoint index, must be < gst_analytics_keypoints_mtd_get_count()
 *
 * Get keypoint position (2D or 3D) for a keypoint at @index
 * Returns: keypoint position for @index, <0.0 if the call failed
 *
 * Since: 1.26
 */
gboolean gst_analytics_keypoints_mtd_get_position(const GstAnalyticsKeypointsMtd *handle, gfloat *position,
                                                  gsize index) {
    g_return_val_if_fail(handle, -1.0);
    g_return_val_if_fail(handle->meta != NULL, -1.0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, -1.0);
    g_return_val_if_fail(keypoints_mtd_data->keypoint_count > index, -1.0);

    float *position_data = (float *)keypoints_mtd_data->data + index * keypoints_mtd_data->keypoint_dimensions;
    for (gsize d = 0; d < keypoints_mtd_data->keypoint_dimensions; d++) {
        position[d] = position_data[d];
    }
    return TRUE;
}

/**
 * gst_analytics_keypoints_mtd_get_confidence:
 * @handle: instance handle
 * @confidence: keypoint confidence (visibility)
 * @index: keypoint index, gst_analytics_keypoints_mtd_get_count()
 *
 * Get keypoint confidence (visibility) for a keypoint at @index
 * Returns: keypoint confidence for @index, <0.0 if the call failed
 *
 * Since: 1.26
 */
gboolean gst_analytics_keypoints_mtd_get_confidence(const GstAnalyticsKeypointsMtd *handle, gfloat *confidence,
                                                    gsize index) {
    g_return_val_if_fail(handle, -1.0);
    g_return_val_if_fail(handle->meta != NULL, -1.0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, -1.0);
    g_return_val_if_fail(keypoints_mtd_data->confidence_count > index, -1.0);

    float *confidence_data = (float *)keypoints_mtd_data->data +
                             keypoints_mtd_data->keypoint_count * keypoints_mtd_data->keypoint_dimensions;

    *confidence = confidence_data[index];

    return TRUE;
}

/**
 * gst_analytics_keypoints_mtd_get_skeleton:
 * @handle: instance handle
 * @skeleton: a pair of keypoint indices representing a skeleton link
 * @index: skeleton link index, must be < gst_analytics_keypoints_mtd_get_skeleton_count
 *
 * Get pair of keypoint indices for a skeleton link at @index
 * Returns: keypoints pair for skeleton @index, <0.0 if the call failed
 *
 * Since: 1.26
 */
gboolean gst_analytics_keypoints_mtd_get_skeleton(const GstAnalyticsKeypointsMtd *handle, GstKeypointPair *skeleton,
                                                  gsize index) {
    g_return_val_if_fail(handle, -1.0);
    g_return_val_if_fail(handle->meta != NULL, -1.0);
    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_get_mtd_data(handle->meta, handle->id);
    g_return_val_if_fail(keypoints_mtd_data != NULL, -1.0);
    g_return_val_if_fail(keypoints_mtd_data->skeleton_count > index, -1.0);

    GstKeypointPair *skeleton_data =
        (GstKeypointPair *)((float *)keypoints_mtd_data->data +
                            keypoints_mtd_data->keypoint_count * keypoints_mtd_data->keypoint_dimensions +
                            keypoints_mtd_data->confidence_count);

    skeleton->kp1 = skeleton_data[index].kp1;
    skeleton->kp2 = skeleton_data[index].kp2;

    return TRUE;
}

/**
 * gst_analytics_relation_meta_add_keypoints_mtd:
 * @instance: Instance of #GstAnalyticsRelationMeta where to add classification instance
 * @keypoint_count: number of keypoints to add
 * @keypoint_dimensions: dimension of keypoint positions (2D or 3D)
 * @positions: (2D array of keypoint_count x keypoint_dimensions):
 * normalized keypoint positions * relative to image or detected object,
 * example: <0.65 (x0), 0.55 (y0), 0.77(x1), 0.1 (x2), ...>
 * @confidence: (array of keypoint_count): confidence level of keypoints,
 * null pointer if per-keypoint confidence data not available,
 * example: <0.1 (invisible), 0.95 (visible), 0.80(visible), ...>
 * @skeleton_count: number of skeleton segments that connect keypoints
 * @skeletons: (array of skeleton_count): keypoint pair indices representing skeleton segments,
 * null pointer if skeleton is not defined
 * example: < {0, 2}, {0, 5}, ...>
 * @keypoint_mtd: (out) (not nullable): Handle updated to newly added keypoint meta.
 *
 * Add analytic keypoint metadata to @instance.
 * Returns: Added successfully
 *
 * Since: 1.26
 */
gboolean gst_analytics_relation_meta_add_keypoints_mtd(GstAnalyticsRelationMeta *instance, const gsize keypoint_count,
                                                       const GstAnalyticsKeypointDimensions keypoint_dimensions,
                                                       const gfloat *positions, const gfloat *confidences,
                                                       const gsize skeleton_count, const GstKeypointPair *skeletons,
                                                       GstAnalyticsKeypointsMtd *keypoint_mtd) {
    g_return_val_if_fail(instance, FALSE);
    g_return_val_if_fail(positions != NULL, FALSE);

    gboolean has_confidences = confidences != NULL;
    gboolean has_skeletons = skeletons != NULL;

    gsize data_size = sizeof(gfloat) * keypoint_count * keypoint_dimensions;
    gsize offset_confidence = data_size;
    if (has_confidences)
        data_size += sizeof(gfloat) * keypoint_count;
    gsize offset_skeletons = data_size;
    if (has_skeletons)
        data_size += sizeof(guint) * skeleton_count * 2;

    gsize size = sizeof(GstAnalyticsKeypointsData) + data_size;

    GstAnalyticsKeypointsData *keypoints_mtd_data =
        (GstAnalyticsKeypointsData *)gst_analytics_relation_meta_add_mtd(instance, &keypoints_impl, size, keypoint_mtd);

    if (keypoints_mtd_data) {
        keypoints_mtd_data->keypoint_count = keypoint_count;
        keypoints_mtd_data->keypoint_dimensions = keypoint_dimensions;
        keypoints_mtd_data->confidence_count = (has_confidences) ? keypoint_count : 0;
        keypoints_mtd_data->skeleton_count = (has_skeletons) ? skeleton_count : 0;

        // fill in keypoint positions, confidence levels and skeletons
        memcpy(keypoints_mtd_data->data, positions, keypoint_count * keypoint_dimensions * sizeof(float));

        if (has_confidences)
            memcpy(keypoints_mtd_data->data + offset_confidence, confidences, keypoint_count * sizeof(float));

        if (has_skeletons)
            memcpy(keypoints_mtd_data->data + offset_skeletons, skeletons, skeleton_count * 2 * sizeof(guint));
    }
    return keypoints_mtd_data != NULL;
}