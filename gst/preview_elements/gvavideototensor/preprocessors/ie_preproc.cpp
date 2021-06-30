/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ie_preproc.hpp"

#include <gva_custom_meta.hpp>

using namespace InferenceEngine;

namespace {

InferenceEngine::ColorFormat gst_to_ie_format(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGR:
        return InferenceEngine::ColorFormat::BGR;
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
        return InferenceEngine::ColorFormat::BGRX;
    case GST_VIDEO_FORMAT_NV12:
        // return InferenceEngine::ColorFormat::NV12;
    case GST_VIDEO_FORMAT_I420:
        // return InferenceEngine::ColorFormat::I420;
    default:
        throw std::runtime_error("Unsupported color format.");
    }
}

int get_channels_num(GstVideoFormat format) {
    switch (format) {
    case GST_VIDEO_FORMAT_BGR:
        return 3;
    case GST_VIDEO_FORMAT_BGRx:
    case GST_VIDEO_FORMAT_BGRA:
        return 4;
    case GST_VIDEO_FORMAT_NV12:
        // return InferenceEngine::ColorFormat::NV12;
    case GST_VIDEO_FORMAT_I420:
        // return InferenceEngine::ColorFormat::I420;
    default:
        throw std::runtime_error("Unsupported color format.");
    }
}

} /* anonymous namespace */

IEPreProc::IEPreProc(GstVideoInfo *video_info) : _pre_proc_info(new InferenceEngine::PreProcessInfo()) {
    if (!video_info)
        throw std::invalid_argument("GstVideoInfo is null");

    auto format = GST_VIDEO_INFO_FORMAT(video_info);
    _pre_proc_info->setResizeAlgorithm(InferenceEngine::ResizeAlgorithm::RESIZE_BILINEAR);
    _pre_proc_info->setColorFormat(gst_to_ie_format(format));
    _channels = get_channels_num(format);
    _width = GST_VIDEO_INFO_WIDTH(video_info);
    _height = GST_VIDEO_INFO_HEIGHT(video_info);
}

void IEPreProc::process(GstBuffer *, GstBuffer *) {
    throw std::runtime_error("IEPreProc: Only in-place processing supported");
}

void IEPreProc::process(GstBuffer *buffer) {
    GstGVACustomMeta *meta = GST_GVA_CUSTOM_META_ADD(buffer);
    g_assert(meta->pre_process_info == nullptr);
    // TODO: who should be responsible for cleanup
    meta->pre_process_info = _pre_proc_info.get();
    // TODO: find better approach to pass this info
    meta->channels = _channels;
    meta->width = _width;
    meta->height = _height;
}
