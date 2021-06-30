/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "renderer_gpu.h"

#define VAAF_API __attribute__((__visibility__("default")))

VAAF_API std::unique_ptr<Renderer> create_renderer(InferenceBackend::FourCC format,
                                                   std::shared_ptr<ColorConverter> converter,
                                                   InferenceBackend::MemoryType memory_type, int width, int height) {
    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        return std::unique_ptr<Renderer>(new gpu::draw::RendererBGR(converter, memory_type, width, height));
    case InferenceBackend::FOURCC_NV12:
        return std::unique_ptr<Renderer>(new gpu::draw::RendererNV12(converter, memory_type, width, height));
    case InferenceBackend::FOURCC_I420:
        return std::unique_ptr<Renderer>(new gpu::draw::RendererI420(converter, memory_type, width, height));
    default:
        throw std::runtime_error("Unsupported format");
    }
    return std::unique_ptr<Renderer>(new gpu::draw::RendererI420(converter, memory_type, width, height));
}
