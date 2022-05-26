/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ie_preproc.hpp"

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
        return InferenceEngine::ColorFormat::NV12;
    case GST_VIDEO_FORMAT_I420:
        return InferenceEngine::ColorFormat::I420;
    default:
        throw std::runtime_error("Unsupported color format.");
    }
}

} /* anonymous namespace */

IEPreProc::IEPreProc(GstVideoInfo *video_info)
    : _video_info(video_info), _pre_proc_info(new InferenceEngine::PreProcessInfo()) {
    if (!video_info)
        throw std::invalid_argument("GstVideoInfo is null");

    auto format = GST_VIDEO_INFO_FORMAT(video_info);
    _pre_proc_info->setResizeAlgorithm(InferenceEngine::ResizeAlgorithm::RESIZE_BILINEAR);
    _pre_proc_info->setColorFormat(gst_to_ie_format(format));
}

void IEPreProc::process(GstBuffer *, GstBuffer *, GstVideoRegionOfInterestMeta *) {
}
