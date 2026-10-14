/*******************************************************************************
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "roi_scale.hpp"

#include <algorithm>
#include <cstring>

namespace gvaroiscale {
namespace {

bool is_target_class(GQuark label, const GArray *target_classes) {
    if (!target_classes || target_classes->len == 0)
        return true;

    const auto *labels = reinterpret_cast<const GQuark *>(target_classes->data);
    return std::find(labels, labels + target_classes->len, label) != labels + target_classes->len;
}

bool first_valid_roi(GstBuffer *buffer, const GArray *target_classes, gint &x, gint &y, gint &width, gint &height) {
    GstAnalyticsRelationMeta *relation_meta = gst_buffer_get_analytics_relation_meta(buffer);
    if (!relation_meta)
        return false;

    gpointer state = nullptr;
    GstAnalyticsODMtd detection;
    while (
        gst_analytics_relation_meta_iterate(relation_meta, &state, gst_analytics_od_mtd_get_mtd_type(), &detection)) {
        if (!is_target_class(gst_analytics_od_mtd_get_obj_type(&detection), target_classes))
            continue;
        if (gst_analytics_od_mtd_get_location(&detection, &x, &y, &width, &height, nullptr) && width > 0 && height > 0)
            return true;
    }
    return false;
}

} // namespace

RoiScaleResult roi_scale_buffer(GstBuffer *input_buffer, const GstVideoInfo &input_info, guint output_width,
                                guint output_height, const GArray *target_classes, GstBuffer **output_buffer,
                                GstVideoInfo *output_info) {
    if (output_buffer)
        *output_buffer = nullptr;
    if (!input_buffer || !output_buffer || output_width == 0 || output_height == 0 ||
        GST_VIDEO_INFO_FORMAT(&input_info) != GST_VIDEO_FORMAT_RGB || GST_VIDEO_INFO_WIDTH(&input_info) == 0 ||
        GST_VIDEO_INFO_HEIGHT(&input_info) == 0)
        return RoiScaleResult::Error;

    gint roi_x = 0, roi_y = 0, roi_width = 0, roi_height = 0;
    if (!first_valid_roi(input_buffer, target_classes, roi_x, roi_y, roi_width, roi_height))
        return RoiScaleResult::NoRoi;

    const gint64 left = std::max<gint64>(roi_x, 0);
    const gint64 top = std::max<gint64>(roi_y, 0);
    const gint64 right = std::min<gint64>(static_cast<gint64>(roi_x) + roi_width, GST_VIDEO_INFO_WIDTH(&input_info));
    const gint64 bottom = std::min<gint64>(static_cast<gint64>(roi_y) + roi_height, GST_VIDEO_INFO_HEIGHT(&input_info));
    if (right <= left || bottom <= top)
        return RoiScaleResult::NoRoi;

    const guint crop_width = static_cast<guint>(right - left);
    const guint crop_height = static_cast<guint>(bottom - top);
    const guint64 width_limited = static_cast<guint64>(output_width) * crop_height;
    const guint64 height_limited = static_cast<guint64>(output_height) * crop_width;
    guint resized_width, resized_height;
    if (width_limited <= height_limited) {
        resized_width = output_width;
        resized_height =
            static_cast<guint>((static_cast<guint64>(crop_height) * output_width + crop_width / 2) / crop_width);
    } else {
        resized_height = output_height;
        resized_width =
            static_cast<guint>((static_cast<guint64>(crop_width) * output_height + crop_height / 2) / crop_height);
    }
    resized_width = std::max(1u, std::min(resized_width, output_width));
    resized_height = std::max(1u, std::min(resized_height, output_height));

    GstVideoInfo scaled_info;
    if (!gst_video_info_set_format(&scaled_info, GST_VIDEO_FORMAT_RGB, output_width, output_height))
        return RoiScaleResult::Error;
    GstBuffer *scaled_buffer = gst_buffer_new_allocate(nullptr, scaled_info.size, nullptr);
    if (!scaled_buffer)
        return RoiScaleResult::Error;
    if (gst_buffer_memset(scaled_buffer, 0, 0, scaled_info.size) != scaled_info.size) {
        gst_buffer_unref(scaled_buffer);
        return RoiScaleResult::Error;
    }

    GstVideoFrame input_frame = {}, output_frame = {};
    if (!gst_video_frame_map(&input_frame, &input_info, input_buffer, GST_MAP_READ)) {
        gst_buffer_unref(scaled_buffer);
        return RoiScaleResult::Error;
    }
    if (!gst_video_frame_map(&output_frame, &scaled_info, scaled_buffer, GST_MAP_WRITE)) {
        gst_video_frame_unmap(&input_frame);
        gst_buffer_unref(scaled_buffer);
        return RoiScaleResult::Error;
    }

    const guint8 *input_pixels = static_cast<const guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&input_frame, 0));
    guint8 *output_pixels = static_cast<guint8 *>(GST_VIDEO_FRAME_PLANE_DATA(&output_frame, 0));
    const gint input_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&input_frame, 0);
    const gint output_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&output_frame, 0);
    const guint offset_x = (output_width - resized_width) / 2;
    const guint offset_y = (output_height - resized_height) / 2;
    for (guint row = 0; row < resized_height; ++row) {
        const guint source_y = static_cast<guint>((static_cast<guint64>(row) * crop_height) / resized_height);
        const guint8 *source_row =
            input_pixels + (static_cast<gssize>(top) + source_y) * input_stride + static_cast<gssize>(left) * 3;
        guint8 *destination_row =
            output_pixels + static_cast<gssize>(offset_y + row) * output_stride + static_cast<gssize>(offset_x) * 3;
        for (guint column = 0; column < resized_width; ++column) {
            const guint source_x = static_cast<guint>((static_cast<guint64>(column) * crop_width) / resized_width);
            std::memcpy(destination_row + static_cast<gsize>(column) * 3,
                        source_row + static_cast<gssize>(source_x) * 3, 3);
        }
    }

    gst_video_frame_unmap(&output_frame);
    gst_video_frame_unmap(&input_frame);
    GST_BUFFER_PTS(scaled_buffer) = GST_BUFFER_PTS(input_buffer);
    GST_BUFFER_DTS(scaled_buffer) = GST_BUFFER_DTS(input_buffer);
    GST_BUFFER_DURATION(scaled_buffer) = GST_BUFFER_DURATION(input_buffer);
    *output_buffer = scaled_buffer;
    if (output_info)
        *output_info = scaled_info;
    return RoiScaleResult::Ok;
}

} // namespace gvaroiscale