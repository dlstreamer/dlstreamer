/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "watermark.h"
#include "config.h"
#include "glib.h"

// #include "render_human_pose.h"
#include "gva_buffer_map.h"
#include "gva_utils.h"
#include "video_frame.h"
#include <gst/allocators/gstdmabuf.h>
#include <opencv2/opencv.hpp>

static const std::vector<cv::Scalar> color_table_C3 = {
    cv::Scalar(255, 0, 0),   cv::Scalar(0, 255, 0),   cv::Scalar(0, 0, 255),   cv::Scalar(255, 255, 0),
    cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 255), cv::Scalar(255, 170, 0), cv::Scalar(255, 0, 170),
    cv::Scalar(0, 255, 170), cv::Scalar(170, 255, 0), cv::Scalar(170, 0, 255), cv::Scalar(0, 170, 255),
    cv::Scalar(255, 85, 0),  cv::Scalar(85, 255, 0),  cv::Scalar(0, 255, 85),  cv::Scalar(0, 85, 255),
    cv::Scalar(85, 0, 255),  cv::Scalar(255, 0, 85)};

static const std::vector<cv::Scalar> color_table_C1 = {cv::Scalar(0), cv::Scalar(255)};

static cv::Scalar index2color(size_t index, int fourcc) {
    if (fourcc == InferenceBackend::FOURCC_I420 or fourcc == InferenceBackend::FOURCC_NV12) {
        return color_table_C1[index % color_table_C1.size()];
    } else {
        cv::Scalar color = color_table_C3[index % color_table_C3.size()];
        if (fourcc == InferenceBackend::FOURCC_RGBA || fourcc == InferenceBackend::FOURCC_RGBX) {
            std::swap(color[0], color[2]);
        }

        return color;
    }
}

int Fourcc2OpenCVType(int fourcc) {
    switch (fourcc) {
    case InferenceBackend::FOURCC_NV12:
        return CV_8UC1; // only Y plane
    case InferenceBackend::FOURCC_BGRA:
        return CV_8UC4;
    case InferenceBackend::FOURCC_BGRX:
        return CV_8UC4;
    case InferenceBackend::FOURCC_BGRP:
        return 0;
    case InferenceBackend::FOURCC_BGR:
        return CV_8UC3;
    case InferenceBackend::FOURCC_RGBA:
        return CV_8UC4;
    case InferenceBackend::FOURCC_RGBX:
        return CV_8UC4;
    case InferenceBackend::FOURCC_RGBP:
        return 0;
    case InferenceBackend::FOURCC_I420:
        return CV_8UC1; // only Y plane
    }
    return 0;
}

void draw_label(GstBuffer *buffer, GstVideoInfo *info) {
    // map GstBuffer to cv::Mat
    InferenceBackend::Image image;
    BufferMapContext mapContext;
    GstMemory *mem = gst_buffer_get_memory(buffer, 0);
    GstMapFlags mapFlags = (mem && gst_is_fd_memory(mem)) ? GST_MAP_READWRITE : GST_MAP_READ; // TODO
    gst_memory_unref(mem);

    gva_buffer_map(buffer, image, mapContext, info, InferenceBackend::MemoryType::SYSTEM, mapFlags);
    int format = Fourcc2OpenCVType(image.format);
    cv::Mat mat(image.height, image.width, format, image.planes[0], info->stride[0]);

    // construct text labels
    GVA::VideoFrame video_frame(buffer, info);
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        std::string text = "";
        size_t color_index = roi.label_id();
        int object_id = 0;

        GstVideoRegionOfInterestMeta *roi_meta = roi.meta();

        get_object_id(roi_meta, &object_id);
        if (object_id > 0) {
            text = std::to_string(object_id) + ": ";
            color_index = object_id;
        }

        if (roi_meta->roi_type) {
            const gchar *type = g_quark_to_string(roi_meta->roi_type);
            if (type) {
                if (!text.empty())
                    text += " ";
                text += std::string(type);
            }
        }
        auto velocity_meta = gst_video_region_of_interest_meta_get_param(roi.meta(), "Velocity");

        if (velocity_meta != nullptr) {
            double velocity = 0;
            double avg_velocity = 0;
            gst_structure_get_double(velocity_meta, "velocity", &velocity);
            gst_structure_get_double(velocity_meta, "avg_velocity", &avg_velocity);
            auto int_velocity = static_cast<int>(velocity);
            auto int_avg_velocity = static_cast<int>(avg_velocity);
            text = std::to_string(object_id) + ", vel: " + std::to_string(int_velocity) +
                   ", avg vel: " + std::to_string(int_avg_velocity);
            auto meta = roi.meta();
            cv::Point2f pos(meta->x, meta->y - 5.f);
            if (pos.y < 0)
                pos.y = meta->y + 30.f;
            auto color_red = cv::Scalar(255, 0, 0);
            cv::putText(mat, text, pos, cv::FONT_HERSHEY_SIMPLEX, 1, color_red, 2);
            // // fprintf(stdout, "%f \n", velocity);
        }
        for (GVA::Tensor &tensor : roi) {
            if (!tensor.is_detection()) {
                std::string label = tensor.label();
                if (!label.empty()) {
                    if (!text.empty())
                        text += " ";
                    text += label;
                }
            }
            // landmarks rendering
            if (tensor.model_name().find("landmarks") != std::string::npos || tensor.format() == "landmark_points") {
                std::vector<float> data = tensor.data<float>();
                for (guint i = 0; i < data.size() / 2; i++) {
                    cv::Scalar color = index2color(i, image.format);
                    int x_lm = roi.meta()->x + roi.meta()->w * data[2 * i];
                    int y_lm = roi.meta()->y + roi.meta()->h * data[2 * i + 1];
                    cv::circle(mat, cv::Point(x_lm, y_lm), 1 + static_cast<int>(0.012 * roi.meta()->w), color, -1);
                }
            }
            // if(tensor.model_name().find("pose") != std::string::npos ||
            //     tensor.get_string("format") == "skeleton_points") {
            //         std::vector<float> data = tensor.data<float>();
            //         renderHumanPose(data, mat);
            //     }
        }

        // draw rectangle
        cv::Scalar color = index2color(color_index, image.format); // TODO: Is it good mapping to colors?

        cv::Point2f bbox_min(roi_meta->x, roi_meta->y);
        cv::Point2f bbox_max(roi_meta->x + roi_meta->w, roi_meta->y + roi_meta->h);
        cv::rectangle(mat, bbox_min, bbox_max, color, 1);

        // put text
        cv::Point2f pos(roi_meta->x, roi_meta->y - 5.f);
        if (pos.y < 0)
            pos.y = roi_meta->y + 30.f;
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_TRIPLEX, 1, color, 1);
    }

    // unmap GstBuffer
    gva_buffer_unmap(buffer, image, mapContext);
}
