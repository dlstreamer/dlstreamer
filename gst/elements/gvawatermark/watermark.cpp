/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
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

extern "C" {
static cv::Scalar index2color(int index) {
    switch (index & 7) {
    case 0:
        return cv::Scalar(0, 0, 255);
    case 1:
        return cv::Scalar(0, 255, 0);
    case 2:
        return cv::Scalar(255, 0, 0);
    case 3:
        return cv::Scalar(0, 255, 255);
    case 4:
        return cv::Scalar(255, 255, 0);
    case 5:
        return cv::Scalar(255, 0, 255);
    case 6:
        return cv::Scalar(127, 127, 127);
    case 7:
        return cv::Scalar(0, 0, 0);
    }
    return cv::Scalar(0, 0, 0);
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
    gva_buffer_map(buffer, image, mapContext, info, InferenceBackend::MemoryType::SYSTEM);
    int format = Fourcc2OpenCVType(image.format);
    cv::Mat mat(image.height, image.width, format, image.planes[0], info->stride[0]);

    // construct text labels
    GVA::RegionOfInterestList roi_list(buffer);
    for (GVA::RegionOfInterest &roi : roi_list) {
        std::string text;
        gint object_id = 0;
        for (GVA::Tensor &tensor : roi) {
            std::string label = tensor.label();
            if (!label.empty()) {
                text += label + " ";
            }
            if (gst_structure_has_field(tensor.gst_structure(), "object_id")) {
                object_id = tensor.object_id();
            }
        }

        GstVideoRegionOfInterestMeta *meta = roi.meta();
        if (meta->roi_type) {
            text += " ";
            text += g_quark_to_string(meta->roi_type);
        }

        // draw rectangle with text
        cv::Scalar color = index2color(meta->roi_type + object_id); // TODO: Is it good mapping to colors?
        cv::Point2f bbox_min(meta->x, meta->y);
        cv::Point2f bbox_max(meta->x + meta->w, meta->y + meta->h);
        cv::rectangle(mat, bbox_min, bbox_max, color, 2);
        cv::Point2f pos(meta->x, meta->y - 5);
        cv::putText(mat, text, pos, cv::FONT_HERSHEY_SIMPLEX, 1, color, 2);
    }

    // unmap GstBuffer
    gva_buffer_unmap(buffer, image, mapContext);
}
} /* extern "C" */
