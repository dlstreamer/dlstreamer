/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "create_renderer.h"

#include "renderer_cpu.h"

std::unique_ptr<Renderer> create_cpu_renderer(InferenceBackend::FourCC format,
                                              std::shared_ptr<ColorConverter> converter,
                                              InferenceBackend::MemoryType memory_type) {
    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        return std::unique_ptr<Renderer>(new RendererBGR(converter, memory_type));
    case InferenceBackend::FOURCC_NV12:
        return std::unique_ptr<Renderer>(new RendererNV12(converter, memory_type));
    case InferenceBackend::FOURCC_I420:
        return std::unique_ptr<Renderer>(new RendererI420(converter, memory_type));
    default:
        throw std::runtime_error("Unsupported format");
    }
}
