/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "watermark.h"
#include "config.h"
#include "gva_buffer_map.h"
#include "gva_roi_meta.h"

#include "glib.h"
#include <gst/allocators/gstdmabuf.h>
#include <opencv2/opencv.hpp>

#define LANDMARKS_POINT_COLOR_INDEX 1
extern "C" {
static cv::Scalar color_table[8] = {cv::Scalar(0, 0, 255),     cv::Scalar(0, 255, 0),   cv::Scalar(255, 0, 0),
                                    cv::Scalar(0, 255, 255),   cv::Scalar(255, 255, 0), cv::Scalar(255, 0, 255),
                                    cv::Scalar(127, 127, 127), cv::Scalar(0, 0, 0)};

static cv::Scalar index2color(int index, int fourcc) {
    int tmp;
    cv::Scalar color(0, 0, 0);

    color = color_table[index & 7];

    if (fourcc == InferenceBackend::FOURCC_RGBA || fourcc == InferenceBackend::FOURCC_RGBX) {
        tmp = color[0];
        color[0] = color[2];
        color[2] = tmp;
    }

    return color;
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
    GVA::RegionOfInterestList roi_list(buffer);
    for (GVA::RegionOfInterest &roi : roi_list) {
        std::string text;
        int object_id = roi.meta()->id;
        int color_index = object_id;

        if (object_id > 0) {
            text = std::to_string(object_id) + ": ";
        }

        for (GVA::Tensor &tensor : roi) {
            std::string label = tensor.label();
            if (!label.empty()) {
                text += label + " ";
            }
            if (tensor.model_name().find("landmarks") != std::string::npos ||
                tensor.get_string("format") == "landmark_points") {
                std::vector<float> data = tensor.data<float>();
                for (guint i = 0; i < data.size() / 2; i++) {
                    int x_lm = roi.meta()->x + roi.meta()->w * data[2 * i];
                    int y_lm = roi.meta()->y + roi.meta()->h * data[2 * i + 1];
                    cv::circle(mat, cv::Point(x_lm, y_lm), 1 + static_cast<int>(0.012 * roi.meta()->w),
                               color_table[LANDMARKS_POINT_COLOR_INDEX], -1);
                }
            }
        }

        GstVideoRegionOfInterestMeta *meta = roi.meta();
        if (meta->roi_type) {
            std::hash<std::string> myhash;
            const gchar *type = g_quark_to_string(meta->roi_type);

            text += type;
            color_index += (int)myhash(type);
        }

        // draw rectangle
        cv::Scalar color = index2color(color_index, image.format); // TODO: Is it good mapping to colors?
        cv::Point2f bbox_min(meta->x, meta->y);
        cv::Point2f bbox_max(meta->x + meta->w, meta->y + meta->h);
        cv::rectangle(mat, bbox_min, bbox_max, color, 2);

        // put text
        cv::Point2f pos(meta->x, meta->y - 5.f);
        if (pos.y < 0)
            pos.y = meta->y + 30.f;
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_SIMPLEX, 1, color, 2);
    }

    // unmap GstBuffer
    gva_buffer_unmap(buffer, image, mapContext);
}
} /* extern "C" */
