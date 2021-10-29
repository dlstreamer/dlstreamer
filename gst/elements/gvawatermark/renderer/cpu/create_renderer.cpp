/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "create_renderer.h"

#include "gva_utils.h"
#include "renderer_cpu.h"

std::unique_ptr<Renderer> create_cpu_renderer(const GstVideoInfo *video_info, std::shared_ptr<ColorConverter> converter,
                                              InferenceBackend::MemoryType memory_type) {

    auto format = static_cast<InferenceBackend::FourCC>(gst_format_to_fourcc(GST_VIDEO_INFO_FORMAT(video_info)));

    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        return std::unique_ptr<Renderer>(new RendererBGR(converter, memory_type, video_info));
    case InferenceBackend::FOURCC_NV12:
        return std::unique_ptr<Renderer>(new RendererNV12(converter, memory_type, video_info));
    case InferenceBackend::FOURCC_I420:
        return std::unique_ptr<Renderer>(new RendererI420(converter, memory_type, video_info));
    default:
        throw std::runtime_error("Unsupported format");
    }
}
