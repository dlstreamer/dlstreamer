/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "coordinates_restorer.h"

#include "copy_blob_to_gststruct.h"
#include "gva_base_inference.h"
#include "gva_utils.h"
#include "processor_types.h"

#include "inference_backend/logger.h"
#include "inference_backend/safe_arithmetic.h"
#include "tensor.h"

#include <cassert>
#include <exception>
#include <memory>
#include <vector>

using namespace post_processing;

void ROICoordinatesRestorer::clipNormalizedRect(double &real_x_min, double &real_y_min, double &real_x_max,
                                                double &real_y_max) {
    if (!((real_x_min >= 0) && (real_y_min >= 0) && (real_x_max < +1) && (real_y_max < +1))) {
        GST_DEBUG("ROI coordinates top_left=[%.5lf, %.5lf], right_bottom=[%.5lf, %.5lf] are out of range [0,1] and "
                  "will be clipped",
                  real_x_min, real_y_min, real_x_max, real_y_max);

        real_x_min = (real_x_min < 0) ? 0 : (real_x_min > 1) ? 1 : real_x_min;
        real_y_min = (real_y_min < 0) ? 0 : (real_y_min > 1) ? 1 : real_y_min;
        real_x_max = (real_x_max < 0) ? 0 : (real_x_max > 1) ? 1 : real_x_max;
        real_y_max = (real_y_max < 0) ? 0 : (real_y_max > 1) ? 1 : real_y_max;
    }
}

void ROICoordinatesRestorer::getAbsoluteCoordinates(int orig_image_width, int orig_image_height, double real_x_min,
                                                    double real_y_min, double real_x_max, double real_y_max,
                                                    uint32_t &abs_x, uint32_t &abs_y, uint32_t &abs_w,
                                                    uint32_t &abs_h) {
    abs_x = safe_convert<uint32_t>(real_x_min * orig_image_width + 0.5);
    abs_y = safe_convert<uint32_t>(real_y_min * orig_image_height + 0.5);
    abs_w = safe_convert<uint32_t>((real_x_max - real_x_min) * orig_image_width + 0.5);
    abs_h = safe_convert<uint32_t>((real_y_max - real_y_min) * orig_image_height + 0.5);
}

void ROICoordinatesRestorer::getActualCoordinates(int orig_image_width, int orig_image_height,
                                                  const InferenceBackend::ImageTransformationParams::Ptr &pre_proc_info,
                                                  double &real_x_min, double &real_y_min, double &real_x_max,
                                                  double &real_y_max) {

    auto input_img_abs_min_x = real_x_min * input_info.width;
    auto input_img_abs_min_y = real_y_min * input_info.height;
    auto input_img_abs_max_x = real_x_max * input_info.width;
    auto input_img_abs_max_y = real_y_max * input_info.height;

    if (pre_proc_info->WasCrop()) {
        input_img_abs_min_x += pre_proc_info->cropped_frame_size_x;
        input_img_abs_min_y += pre_proc_info->cropped_frame_size_y;
        input_img_abs_max_x += pre_proc_info->cropped_frame_size_x;
        input_img_abs_max_y += pre_proc_info->cropped_frame_size_y;
    }

    if (pre_proc_info->WasAspectRatioResize() or pre_proc_info->WasPadding()) {
        auto abs_min_x = (input_img_abs_min_x - static_cast<double>(pre_proc_info->resize_padding_size_x)) /
                         pre_proc_info->resize_scale_x;
        auto abs_min_y = (input_img_abs_min_y - static_cast<double>(pre_proc_info->resize_padding_size_y)) /
                         pre_proc_info->resize_scale_y;
        auto abs_max_x = (input_img_abs_max_x - static_cast<double>(pre_proc_info->resize_padding_size_x)) /
                         pre_proc_info->resize_scale_x;
        auto abs_max_y = (input_img_abs_max_y - static_cast<double>(pre_proc_info->resize_padding_size_y)) /
                         pre_proc_info->resize_scale_y;

        real_x_min = abs_min_x / orig_image_width;
        real_x_max = abs_max_x / orig_image_width;
        real_y_min = abs_min_y / orig_image_height;
        real_y_max = abs_max_y / orig_image_height;
    } else {
        real_x_min += static_cast<double>(pre_proc_info->cropped_frame_size_x) / orig_image_width;
        real_x_max += static_cast<double>(pre_proc_info->cropped_frame_size_x) / orig_image_width;
        real_y_min += static_cast<double>(pre_proc_info->cropped_frame_size_y) / orig_image_height;
        real_y_max += static_cast<double>(pre_proc_info->cropped_frame_size_y) / orig_image_height;
    }
}

/**
 * Iterating through GstBuffer's tensors_batch and searching for meta that matching frame's ROI.
 *
 * @param[in] frame - pointer to InferenceFrame containing pointers to buffer and ROI.
 *
 * @return GstVideoRegionOfInterestMeta - meta of GstBuffer, or nullptr.
 *
 * @throw std::invalid_argument when GstBuffer is nullptr.
 */
GstVideoRegionOfInterestMeta *ROICoordinatesRestorer::findDetectionMeta(const InferenceFrame &frame) {
    GstBuffer *buffer = frame.buffer;
    if (not buffer)
        throw std::invalid_argument("Inference frame's buffer is nullptr");
    auto frame_roi = &frame.roi;
    GstVideoRegionOfInterestMeta *meta = nullptr;
    gpointer state = nullptr;
    while ((meta = GST_VIDEO_REGION_OF_INTEREST_META_ITERATE(buffer, &state))) {
        if (sameRegion(meta, frame_roi)) {
            return meta;
        }
    }
    return meta;
}

void ROICoordinatesRestorer::updateCoordinatesToFullFrame(double &x_min, double &y_min, double &x_max, double &y_max,
                                                          const InferenceFrame &frame) {
    /* In case of gvadetect with inference-region=roi-list we get coordinates relative to ROI.
     * We need to convert them to coordinates relative to the full frame. */
    if (frame.gva_base_inference->inference_region == ROI_LIST) {
        GstVideoRegionOfInterestMeta *meta = findDetectionMeta(frame);
        if (meta) {
            x_min = (meta->x + meta->w * x_min) / frame.info->width;
            y_min = (meta->y + meta->h * y_min) / frame.info->height;
            x_max = (meta->x + meta->w * x_max) / frame.info->width;
            y_max = (meta->y + meta->h * y_max) / frame.info->height;
        }
    }
}

void ROICoordinatesRestorer::getRealCoordinates(GstStructure *detection_tensor, double &x_min_real, double &y_min_real,
                                                double &x_max_real, double &y_max_real) {
    gst_structure_get(detection_tensor, "x_min", G_TYPE_DOUBLE, &x_min_real, "x_max", G_TYPE_DOUBLE, &x_max_real,
                      "y_min", G_TYPE_DOUBLE, &y_min_real, "y_max", G_TYPE_DOUBLE, &y_max_real, NULL);
}

void ROICoordinatesRestorer::getCoordinates(GstStructure *detection_tensor, const InferenceFrame &frame,
                                            double &x_min_real, double &y_min_real, double &x_max_real,
                                            double &y_max_real, uint32_t &x_abs, uint32_t &y_abs, uint32_t &w_abs,
                                            uint32_t &h_abs) {

    getRealCoordinates(detection_tensor, x_min_real, y_min_real, x_max_real, y_max_real);

    if (frame.image_transform_info and frame.image_transform_info->WasTransformation())
        getActualCoordinates(frame.info->width, frame.info->height, frame.image_transform_info, x_min_real, y_min_real,
                             x_max_real, y_max_real);

    updateCoordinatesToFullFrame(x_min_real, y_min_real, x_max_real, y_max_real, frame);

    clipNormalizedRect(x_min_real, y_min_real, x_max_real, y_max_real);

    getAbsoluteCoordinates(frame.info->width, frame.info->height, x_min_real, y_min_real, x_max_real, y_max_real, x_abs,
                           y_abs, w_abs, h_abs);
}

void ROICoordinatesRestorer::restore(TensorsTable &tensors_batch, const InferenceFrames &frames) {
    try {

        if (frames.empty()) {
            throw std::invalid_argument("There are no inference frames");
        }

        if (frames.size() != tensors_batch.size())
            throw std::logic_error("Size of the metadata array does not match the size of the inference frames: " +
                                   std::to_string(tensors_batch.size()) + " / " + std::to_string(frames.size()));

        for (size_t i = 0; i < frames.size(); ++i) {
            const auto &frame = *frames[i].get();
            const auto &tensor = tensors_batch[i];

            for (size_t j = 0; j < tensor.size(); ++j) {
                GstStructure *detection_tensor = tensor[j];

                double x_min_real, y_min_real, x_max_real, y_max_real;
                uint32_t x_abs, y_abs, w_abs, h_abs;
                getCoordinates(detection_tensor, frame, x_min_real, y_min_real, x_max_real, y_max_real, x_abs, y_abs,
                               w_abs, h_abs);

                gst_structure_set(detection_tensor, "x_min", G_TYPE_DOUBLE, x_min_real, "x_max", G_TYPE_DOUBLE,
                                  x_max_real, "y_min", G_TYPE_DOUBLE, y_min_real, "y_max", G_TYPE_DOUBLE, y_max_real,
                                  "x_abs", G_TYPE_UINT, x_abs, "y_abs", G_TYPE_UINT, y_abs, "w_abs", G_TYPE_UINT, w_abs,
                                  "h_abs", G_TYPE_UINT, h_abs, NULL);
            }
        }
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
}

void KeypointsCoordinatesRestorer::restoreActualCoordinates(const InferenceFrame &frame, float &real_x, float &real_y) {
    const InferenceBackend::ImageTransformationParams::Ptr pre_proc_info = frame.image_transform_info;
    const size_t orig_image_width = frame.roi.w;
    const size_t orig_image_height = frame.roi.h;

    if (not(pre_proc_info and pre_proc_info->WasTransformation()))
        return;

    auto input_img_abs_x = real_x * input_info.width;
    auto input_img_abs_y = real_y * input_info.height;

    if (pre_proc_info->WasCrop()) {
        input_img_abs_x += pre_proc_info->cropped_frame_size_x;
        input_img_abs_y += pre_proc_info->cropped_frame_size_y;
    }
    if (pre_proc_info->WasAspectRatioResize() or pre_proc_info->WasPadding()) {

        auto abs_x = (input_img_abs_x - safe_convert<float>(pre_proc_info->resize_padding_size_x)) /
                     pre_proc_info->resize_scale_x;
        auto abs_y = (input_img_abs_y - safe_convert<float>(pre_proc_info->resize_padding_size_y)) /
                     pre_proc_info->resize_scale_y;

        real_x = abs_x / orig_image_width;
        real_y = abs_y / orig_image_height;

    } else {
        real_x += static_cast<float>(pre_proc_info->cropped_frame_size_x) / orig_image_width;
        real_y += static_cast<float>(pre_proc_info->cropped_frame_size_y) / orig_image_height;
    }
}

void KeypointsCoordinatesRestorer::restore(TensorsTable &tensors, const InferenceFrames &frames) {
    try {

        if (frames.empty()) {
            throw std::invalid_argument("There are no inference frames");
        }

        if (frames.size() != tensors.size())
            throw std::logic_error("Size of the metadata array does not match the size of the inference frames: " +
                                   std::to_string(tensors.size()) + " / " + std::to_string(frames.size()));

        for (size_t i = 0; i < frames.size(); ++i) {
            const auto &frame = *frames[i].get();
            const auto &tensor = tensors[i];

            for (size_t j = 0; j < tensor.size(); ++j) {
                GstStructure *result_tensor_raw = tensor[j];
                GVA::Tensor result_tensor(result_tensor_raw);

                auto data = result_tensor.data<float>();

                if (data.empty())
                    throw std::runtime_error("Keypoints is empty.");

                const auto &dimention = result_tensor.dims();
                size_t points_num = dimention[0];
                size_t point_dimension = dimention[1];

                if (data.size() != points_num * point_dimension)
                    throw std::logic_error("The size of the keypoints data does not match the dimension: Size=" +
                                           std::to_string(data.size()) + " Dimension=[" + std::to_string(dimention[0]) +
                                           "," + std::to_string(dimention[1]) + "].");

                auto &updated_points = data;
                for (size_t i = 0; i < points_num; ++i) {
                    if (data[i * point_dimension] == -1.0f and data[i * point_dimension + 1] == -1.0f)
                        continue;

                    restoreActualCoordinates(frame, updated_points[i * point_dimension],
                                             updated_points[i * point_dimension + 1]);
                }

                copy_buffer_to_structure(result_tensor_raw, reinterpret_cast<const void *>(updated_points.data()),
                                         updated_points.size() * sizeof(float));
            }
        }
    } catch (const std::exception &e) {
        GVA_ERROR(e.what());
    }
}
