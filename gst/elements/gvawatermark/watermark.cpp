/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "watermark.h"
#include "config.h"
#include "glib.h"
#include "gva_buffer_map.h"
#include "renderer/renderer_bgr.h"
#include "renderer/renderer_i420.h"
#include "renderer/renderer_nv12.h"
#include "utils.h"
#include "video_frame.h"

#include <exception>
#include <gst/allocators/gstdmabuf.h>
#include <gst/video/video-color.h>
#include <gst/video/video-info.h>
#include <opencv2/opencv.hpp>

using Color = cv::Scalar;

static const std::vector<cv::Scalar> color_table = {
    cv::Scalar(255, 0, 0),   cv::Scalar(0, 255, 0),   cv::Scalar(0, 0, 255),   cv::Scalar(255, 255, 0),
    cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 255), cv::Scalar(255, 170, 0), cv::Scalar(255, 0, 170),
    cv::Scalar(0, 255, 170), cv::Scalar(170, 255, 0), cv::Scalar(170, 0, 255), cv::Scalar(0, 170, 255),
    cv::Scalar(255, 85, 0),  cv::Scalar(85, 255, 0),  cv::Scalar(0, 255, 85),  cv::Scalar(0, 85, 255),
    cv::Scalar(85, 0, 255),  cv::Scalar(255, 0, 85)};
static const cv::Scalar default_color = cv::Scalar(0, 255, 0);

static GstVideoColorMatrix current_colormatrix = GstVideoColorMatrix::GST_VIDEO_COLOR_MATRIX_UNKNOWN;
static std::shared_ptr<Renderer> renderer;

std::shared_ptr<Renderer> createRenderer(InferenceBackend::FourCC format,
                                         const std::vector<cv::Scalar> &rgb_color_table, double Kr, double Kb) {
    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
        return std::make_shared<RendererBGR>(RendererBGR(rgb_color_table));
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        return std::make_shared<RendererRGB>(RendererRGB(rgb_color_table));
    case InferenceBackend::FOURCC_NV12:
        return std::make_shared<RendererNV12>(RendererNV12(rgb_color_table, Kr, Kb));
    case InferenceBackend::FOURCC_I420:
        return std::make_shared<RendererI420>(RendererI420(rgb_color_table, Kr, Kb));
    default:
        throw std::runtime_error("Unsupported format");
    }
}

inline InferenceBackend::FourCC gstFormatToFourCC(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_NV12:
        GST_DEBUG("GST_VIDEO_FORMAT_NV12");
        return InferenceBackend::FourCC::FOURCC_NV12;
    case GST_VIDEO_FORMAT_BGR:
        GST_DEBUG("GST_VIDEO_FORMAT_BGR");
        return InferenceBackend::FourCC::FOURCC_BGR;
    case GST_VIDEO_FORMAT_BGRx:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRx");
        return InferenceBackend::FourCC::FOURCC_BGRX;
    case GST_VIDEO_FORMAT_BGRA:
        GST_DEBUG("GST_VIDEO_FORMAT_BGRA");
        return InferenceBackend::FourCC::FOURCC_BGRA;
    case GST_VIDEO_FORMAT_RGBA:
        GST_DEBUG("GST_VIDEO_FORMAT_RGBA");
        return InferenceBackend::FourCC::FOURCC_RGBA;
    case GST_VIDEO_FORMAT_I420:
        GST_DEBUG("GST_VIDEO_FORMAT_I420");
        return InferenceBackend::FourCC::FOURCC_I420;
    default:
        throw std::runtime_error("Unsupported GST Format");
    }

    GST_WARNING("Unsupported GST Format: %d.", format);
}

void init(GstVideoInfo *info) {
    try {
        if (GST_VIDEO_INFO_COLORIMETRY(info).matrix == GstVideoColorMatrix::GST_VIDEO_COLOR_MATRIX_UNKNOWN) {
            throw std::runtime_error("GST_VIDEO_COLOR_MATRIX_UNKNOWN");
        }
        if (GST_VIDEO_INFO_COLORIMETRY(info).matrix != current_colormatrix) {
            GstVideoColorimetry colorimetry = GST_VIDEO_INFO_COLORIMETRY(info);
            double Kb, Kr;
            gst_video_color_matrix_get_Kr_Kb(colorimetry.matrix, &Kr, &Kb);
            current_colormatrix = colorimetry.matrix;
            renderer = createRenderer(gstFormatToFourCC(GST_VIDEO_INFO_FORMAT(info)), color_table, Kr, Kb);
        }
    } catch (const std::exception &e) {
        std::string err_msg = "Watermark initialization failed: " + std::string(e.what());
        std::throw_with_nested(std::runtime_error(err_msg));
    }
}

static cv::Scalar indexToColor(size_t index) {
    cv::Scalar color = color_table[index % color_table.size()];
    return color;
}

int FourccToOpenCVType(int fourcc) {
    switch (fourcc) {
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
    }
    return 0;
}

static void clip_rect(double &x, double &y, double &w, double &h, GstVideoInfo *info) {
    x = (x < 0) ? 0 : (x > info->width) ? info->width : x;
    y = (y < 0) ? 0 : (y > info->height) ? info->height : y;
    w = (w < 0) ? 0 : (x + w > info->width) ? info->width - x : w;
    h = (h < 0) ? 0 : (y + h > info->height) ? info->height - y : h;
}

std::vector<std::shared_ptr<cv::Mat>> convertImageToMat(const InferenceBackend::Image &image) {

    std::vector<std::shared_ptr<cv::Mat>> image_planes;
    switch (image.format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        image_planes.emplace_back(std::make_shared<cv::Mat>(
            cv::Mat(image.height, image.width, FourccToOpenCVType(image.format), image.planes[0], image.stride[0])));
        break;
    case InferenceBackend::FOURCC_I420:
        image_planes.emplace_back(
            std::make_shared<cv::Mat>(cv::Mat(image.height, image.width, CV_8UC1, image.planes[0], image.stride[0])));
        image_planes.emplace_back(std::make_shared<cv::Mat>(
            cv::Mat(image.height / 2, image.width / 2, CV_8UC1, image.planes[1], image.stride[1])));
        image_planes.emplace_back(std::make_shared<cv::Mat>(
            cv::Mat(image.height / 2, image.width / 2, CV_8UC1, image.planes[2], image.stride[2])));
        break;
    case InferenceBackend::FOURCC_NV12:
        image_planes.emplace_back(
            std::make_shared<cv::Mat>(cv::Mat(image.height, image.width, CV_8UC1, image.planes[0], image.stride[0])));
        image_planes.emplace_back(std::make_shared<cv::Mat>(
            cv::Mat(image.height / 2, image.width / 2, CV_8UC2, image.planes[1], image.stride[1])));
        break;
    default:
        throw std::runtime_error("Unsupported image format");
    }

    return image_planes;
}

/**
 * Draws landmarks on image_planes using given tensor's info.
 */
void draw_landmarks(const std::vector<std::shared_ptr<cv::Mat>> &image_planes, const GVA::Tensor &tensor,
                    GVA::Rect<double> &rectangle) {
    if (tensor.model_name().find("landmarks") != std::string::npos || tensor.format() == "landmark_points") {
        std::vector<float> data = tensor.data<float>();
        for (guint i = 0; i < data.size() / 2; i++) {
            cv::Scalar color = indexToColor(i);
            int x_lm = rectangle.x + rectangle.w * data[2 * i];
            int y_lm = rectangle.y + rectangle.h * data[2 * i + 1];
            size_t radius = 1 + static_cast<int>(0.012 * rectangle.w);
            renderer->draw_circle(image_planes, color, cv::Point2i(x_lm, y_lm), radius);
        }
    }
}

/**
 * Draws given tensors' info on image_planes.
 */
std::string draw_tensors(const std::vector<std::shared_ptr<cv::Mat>> &image_planes,
                         const std::vector<GVA::Tensor> &tensors, GVA::Rect<double> &rectangle) {
    std::string text;
    for (const GVA::Tensor &tensor : tensors) {
        if (!tensor.is_detection()) {
            std::string label = tensor.label();
            if (!label.empty()) {
                text += label + " ";
            }
        }
        /* landmarks rendering */
        draw_landmarks(image_planes, tensor, rectangle);
    }
    return text;
}

/**
 * Draws frame's ROI metas info on image_planes.
 * May change the given GVA::VideoFrame and add detection tensor if it doesn't exist
 * (due to GVA::VideoFrame's normalized_rect() implementation)
 */
void draw_frame_ROIs(const std::vector<std::shared_ptr<cv::Mat>> &image_planes, GVA::VideoFrame &video_frame,
                     GstVideoInfo *info) {
    for (GVA::RegionOfInterest &roi : video_frame.regions()) {
        std::string text = "";
        size_t color_index = roi.label_id();

        auto rect = roi.normalized_rect();
        if (rect.w && rect.h) {
            rect.x *= info->width;
            rect.y *= info->height;
            rect.w *= info->width;
            rect.h *= info->height;
        } else {
            auto rect_u32 = roi.rect();
            rect = {(double)rect_u32.x, (double)rect_u32.y, (double)rect_u32.w, (double)rect_u32.h};
        }
        clip_rect(rect.x, rect.y, rect.w, rect.h, info);

        int object_id = roi.object_id();
        if (object_id > 0) {
            text = std::to_string(object_id) + ": ";
            color_index = object_id;
        }

        if (!roi.label().empty()) {
            text += roi.label() + " ";
        }

        const std::vector<GVA::Tensor> tensors = roi.tensors();
        text += draw_tensors(image_planes, tensors, rect);

        /* draw rectangle */
        cv::Scalar color = indexToColor(color_index); // TODO: Is it good mapping to colors?

        cv::Point2f bbox_min(rect.x, rect.y);
        cv::Point2f bbox_max(rect.x + rect.w, rect.y + rect.h);
        renderer->draw_rectangle(image_planes, color, bbox_min, bbox_max);

        /* put text */
        cv::Point2f pos(rect.x, rect.y - 5.f);
        if (pos.y < 0)
            pos.y = rect.y + 30.f;
        renderer->draw_text(image_planes, color, pos, text);
    }
}

/**
 * Draws frame's tensor metas info on image_planes.
 * Assuming that tensor metas attached to the frame are related to full-frame inference.
 */
void draw_frame_tensors(const std::vector<std::shared_ptr<cv::Mat>> &image_planes, const GVA::VideoFrame &video_frame,
                        GstVideoInfo *info) {
    if (video_frame.tensors().size() == 0)
        return;
    /* tensor metas attached to the frame are supposed to relate to full-frame inference */
    auto rect = GVA::Rect<double>{0, 0, static_cast<double>(info->width), static_cast<double>(info->height)};
    const std::vector<GVA::Tensor> tensors = video_frame.tensors();
    std::string text = draw_tensors(image_planes, tensors, rect);
    /* draw text on hardcoded position as long as we know that tensor relate to full-frame inference */
    cv::Point2f pos(0, 25.f);
    renderer->draw_text(image_planes, default_color, pos, text);
}

/**
 * Draws boxes, labels, landmarks from buffer's tensors and ROIs
 */
gboolean draw(GstGvaWatermark *gvawatermark, GstBuffer *buffer) {
    // map GstBuffer to cv::Mat
    InferenceBackend::Image image;
    BufferMapContext mapContext;

    try {
        gva_buffer_map(buffer, image, mapContext, &gvawatermark->info, InferenceBackend::MemoryType::SYSTEM,
                       GST_MAP_READWRITE);
        auto mapContextPtr = std::unique_ptr<BufferMapContext, std::function<void(BufferMapContext *)>>(
            &mapContext, [&](BufferMapContext *mapContext) { gva_buffer_unmap(*mapContext); });

        std::vector<std::shared_ptr<cv::Mat>> image_planes = convertImageToMat(image);
        GVA::VideoFrame video_frame(buffer, &gvawatermark->info);
        /* process video_frame's ROI metas */
        draw_frame_ROIs(image_planes, video_frame, &gvawatermark->info);
        /* process video_frame's tensors metas */
        draw_frame_tensors(image_planes, video_frame, &gvawatermark->info);
    } catch (const std::exception &e) {
        GST_ELEMENT_ERROR(gvawatermark, STREAM, FAILED, ("watermark has failed to draw label"),
                          ("%s", Utils::createNestedErrorMsg(e).c_str()));
        return FALSE;
    }
    return TRUE;
}
