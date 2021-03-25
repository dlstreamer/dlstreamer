/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
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

ModelInputInfo Converter::input_info;

void Converter::getLabelByLabelId(GValueArray *labels, int label_id, gchar **out_label) {
    *out_label = nullptr;
    if (labels && label_id >= 0 && label_id < (gint)labels->n_values) {
        *out_label = g_value_dup_string(labels->values + label_id);
    }
}

void Converter::clipNormalizedRect(float &x, float &y, float &w, float &h) {
    if (!((x >= 0) && (y >= 0) && (w >= 0) && (h >= 0) && (x + w <= 1) && (y + h <= 1))) {
        GST_DEBUG("ROI coordinates x=[%.5f, %.5f], y=[%.5f, %.5f] are out of range [0,1] and will be clipped", x, x + w,
                  y, y + h);

        x = (x < 0) ? 0 : (x > 1) ? 1 : x;
        y = (y < 0) ? 0 : (y > 1) ? 1 : y;
        w = (w < 0) ? 0 : (w > 1 - x) ? 1 - x : w;
        h = (h < 0) ? 0 : (h > 1 - y) ? 1 - y : h;
    }
}

void Converter::getActualCoordinates(int orig_image_width, int orig_image_height,
                                     const InferenceBackend::ImageTransformationParams::Ptr &pre_proc_info,
                                     float &real_x, float &real_y, float &real_w, float &real_h, uint32_t &abs_x,
                                     uint32_t &abs_y, uint32_t &abs_w, uint32_t &abs_h) {
    if (pre_proc_info) {
        if (pre_proc_info->WasTransformation()) {
            auto input_img_abs_min_x = safe_convert<uint32_t>(real_x * input_info.width + 0.5);
            auto input_img_abs_min_y = safe_convert<uint32_t>(real_y * input_info.height + 0.5);
            auto input_img_abs_max_x = safe_convert<uint32_t>((real_x + real_w) * input_info.width + 0.5);
            auto input_img_abs_max_y = safe_convert<uint32_t>((real_y + real_h) * input_info.height + 0.5);
            if (pre_proc_info->WasCrop()) {
                input_img_abs_min_x += pre_proc_info->cropped_frame_size_x;
                input_img_abs_min_y += pre_proc_info->cropped_frame_size_y;
                input_img_abs_max_x += pre_proc_info->cropped_frame_size_x;
                input_img_abs_max_y += pre_proc_info->cropped_frame_size_y;
            }
            if (pre_proc_info->WasAspectRatioResize()) {
                abs_x = safe_convert<uint32_t>(
                    static_cast<double>(input_img_abs_min_x - pre_proc_info->resize_padding_size_x) /
                        pre_proc_info->resize_scale_x +
                    0.5);
                abs_y = safe_convert<uint32_t>(
                    static_cast<double>(input_img_abs_min_y - pre_proc_info->resize_padding_size_y) /
                        pre_proc_info->resize_scale_y +
                    0.5);
                auto abs_max_x = safe_convert<uint32_t>(
                    static_cast<double>(input_img_abs_max_x - pre_proc_info->resize_padding_size_x) /
                        pre_proc_info->resize_scale_x +
                    0.5);
                auto abs_max_y = safe_convert<uint32_t>(
                    static_cast<double>(input_img_abs_max_y - pre_proc_info->resize_padding_size_y) /
                        pre_proc_info->resize_scale_y +
                    0.5);

                abs_w = abs_max_x - abs_x;
                abs_h = abs_max_y - abs_y;

                real_x = static_cast<float>(abs_x) / orig_image_width;
                real_y = static_cast<float>(abs_y) / orig_image_height;
                real_w = static_cast<float>(abs_w) / orig_image_width;
                real_h = static_cast<float>(abs_h) / orig_image_height;
                clipNormalizedRect(real_x, real_y, real_w, real_h);
            } else {
                abs_x = input_img_abs_min_x;
                abs_y = input_img_abs_min_y;
                abs_w = input_img_abs_max_x - input_img_abs_min_x;
                abs_h = input_img_abs_max_y - input_img_abs_min_y;

                real_x += pre_proc_info->cropped_frame_size_x / orig_image_width;
                real_y += pre_proc_info->cropped_frame_size_y / orig_image_height;
                clipNormalizedRect(real_x, real_y, real_w, real_h);
            }
            return;
        }
    }

    abs_x = safe_convert<uint32_t>(real_x * orig_image_width + 0.5);
    abs_y = safe_convert<uint32_t>(real_y * orig_image_height + 0.5);
    abs_w = safe_convert<uint32_t>(real_w * orig_image_width + 0.5);
    abs_h = safe_convert<uint32_t>(real_h * orig_image_height + 0.5);
}

/**
 * Compares to metas of type GstVideoRegionOfInterestMeta by roi_type and coordinates.
 *
 * @param[in] left - pointer to first GstVideoRegionOfInterestMeta operand.
 * @param[in] right - pointer to second GstVideoRegionOfInterestMeta operand.
 *
 * @return true if given metas are equal, false otherwise.
 */
bool sameRegion(GstVideoRegionOfInterestMeta *left, GstVideoRegionOfInterestMeta *right) {
    return left->roi_type == right->roi_type && left->x == right->x && left->y == right->y && left->w == right->w &&
           left->h == right->h;
}

/**
 * Iterating through GstBuffer's metas and searching for meta that matching frame's ROI.
 *
 * @param[in] frame - pointer to InferenceFrame containing pointers to buffer and ROI.
 *
 * @return GstVideoRegionOfInterestMeta - meta of GstBuffer, or nullptr.
 *
 * @throw std::invalid_argument when GstBuffer is nullptr.
 */
GstVideoRegionOfInterestMeta *findDetectionMeta(InferenceFrame *frame) {
    GstBuffer *buffer = frame->buffer;
    if (not buffer)
        throw std::invalid_argument("Inference frame's buffer is nullptr");
    auto frame_roi = &frame->roi;
    GstVideoRegionOfInterestMeta *meta = NULL;
    gpointer state = NULL;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, frame_roi)) {
            return meta;
        }
    }
    return meta;
}
void updateCoordinatesToFullFrame(gfloat &x, gfloat &y, gfloat &w, gfloat &h, InferenceFrame *frame) {
    /* In case of gvadetect with inference-region=roi-list we get coordinates relative to ROI.
     * We need to convert them to coordinates relative to the full frame. */
    if (frame->gva_base_inference->inference_region == ROI_LIST) {
        GstVideoRegionOfInterestMeta *meta = findDetectionMeta(frame);
        if (meta) {
            x = (meta->x + meta->w * x) / frame->info->width;
            y = (meta->y + meta->h * y) / frame->info->height;
            w = (meta->w * w) / frame->info->width;
            h = (meta->h * h) / frame->info->height;
        }
    }
}

void Converter::addRoi(const std::shared_ptr<InferenceFrame> frame, float x, float y, float w, float h, int label_id,
                       double confidence, GstStructure *detection_tensor, GValueArray *labels) {

    clipNormalizedRect(x, y, w, h);

    gchar *label = nullptr;
    getLabelByLabelId(labels, label_id, &label);

    uint32_t _x, _y, _w, _h;
    getActualCoordinates(frame->info->width, frame->info->height, frame->image_transform_info, x, y, w, h, _x, _y, _w,
                         _h);
    updateCoordinatesToFullFrame(x, y, w, h, frame.get());
    gva_buffer_check_and_make_writable(&frame->buffer, __PRETTY_FUNCTION__);

    GstVideoRegionOfInterestMeta *meta =
        gst_buffer_add_video_region_of_interest_meta(frame->buffer, label, _x, _y, _w, _h);
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

Converter *Converter::create(const GstStructure *output_model_proc_info, const ModelInputInfo &input_info) {
    Converter::input_info = input_info;

    auto converter_type = getConverterType(output_model_proc_info);

    if (converter_type == "DetectionOutput") {
        return new SSDConverter();
    }
    if (converter_type == "tensor_to_bbox_ssd") {
        return new SSDConverter();
    }

    if (converter_type == "tensor_to_bbox_yolo_v2" or converter_type == "tensor_to_bbox_yolo_v3") {
        return YOLOConverter::makeYOLOConverter(converter_type, output_model_proc_info, input_info);
    }
    return nullptr;
}
