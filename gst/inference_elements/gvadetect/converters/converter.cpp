/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#include "converter.h"
#include "converters/ssd.h"
#include "converters/yolo_base.h"

#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"

using namespace DetectionPlugin;
using namespace Converters;

void Converter::getLabelByLabelId(GValueArray *labels, int label_id, gchar **out_label) {
    *out_label = nullptr;
    if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
        *out_label = g_value_dup_string(labels->values + label_id);
    }
}

void Converter::clipNormalizedRect(double &x, double &y, double &w, double &h) {
    if (!((x >= 0) && (y >= 0) && (w >= 0) && (h >= 0) && (x + w <= 1) && (y + h <= 1))) {
        GST_DEBUG("ROI coordinates x=[%.5f, %.5f], y=[%.5f, %.5f] are out of range [0,1] and will be clipped", x, x + w,
                  y, y + h);

        x = (x < 0) ? 0 : (x > 1) ? 1 : x;
        y = (y < 0) ? 0 : (y > 1) ? 1 : y;
        w = (w < 0) ? 0 : (w > 1 - x) ? 1 - x : w;
        h = (h < 0) ? 0 : (h > 1 - y) ? 1 - y : h;
    }
}

void Converter::addRoi(GstBuffer *buffer, GstVideoInfo *info, double x, double y, double w, double h, int label_id,
                       double confidence, GstStructure *detection_tensor, GValueArray *labels) {
    clipNormalizedRect(x, y, w, h);

    gchar *label = nullptr;
    getLabelByLabelId(labels, label_id, &label);

    uint32_t _x = safe_convert<uint32_t>(x * info->width + 0.5);
    uint32_t _y = safe_convert<uint32_t>(y * info->height + 0.5);
    uint32_t _w = safe_convert<uint32_t>(w * info->width + 0.5);
    uint32_t _h = safe_convert<uint32_t>(h * info->height + 0.5);
    GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(buffer, label, _x, _y, _w, _h);
    g_free(label);
    if (not meta)
        throw std::runtime_error("Failed to add GstVideoRegionOfInterestMeta to buffer");

    gst_structure_set_name(detection_tensor, "detection"); // make sure name="detection"
    gst_structure_set(detection_tensor, "label_id", G_TYPE_INT, label_id, "confidence", G_TYPE_DOUBLE, confidence,
                      "x_min", G_TYPE_DOUBLE, x, "x_max", G_TYPE_DOUBLE, x + w, "y_min", G_TYPE_DOUBLE, y, "y_max",
                      G_TYPE_DOUBLE, y + h, NULL);
    gst_video_region_of_interest_meta_add_param(meta, detection_tensor);
}

constexpr char DEFAULT_CONVERTER_TYPE[] = "tensor_to_bbox_ssd";
std::string Converter::getConverterType(const GstStructure *s) {
    if (s == nullptr || !gst_structure_has_field(s, "converter"))
        return DEFAULT_CONVERTER_TYPE;
    auto converter_type = gst_structure_get_string(s, "converter");
    if (converter_type == nullptr)
        throw std::runtime_error("model_proc's output_processor has empty converter");
    return converter_type;
}

Converter *Converter::create(const GstStructure *model_proc_info) {
    auto converter_type = getConverterType(model_proc_info);

    if (converter_type == "DetectionOutput") {
        return new SSDConverter();
    }
    if (converter_type == "tensor_to_bbox_ssd") {
        return new SSDConverter();
    }

    if (converter_type == "tensor_to_bbox_yolo_v2" or converter_type == "tensor_to_bbox_yolo_v3") {
        return YOLOConverter::makeYOLOConverter(converter_type, model_proc_info);
    }
    return nullptr;
}
