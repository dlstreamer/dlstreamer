/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <buffer_map/buffer_mapper.h>

#include "renderer_gpu.h"

#define VAAF_API __attribute__((__visibility__("default")))

VAAF_API std::unique_ptr<Renderer> create_renderer(InferenceBackend::FourCC format,
                                                   std::shared_ptr<ColorConverter> converter,
                                                   std::unique_ptr<BufferMapper> input_buffer_mapper, int width,
                                                   int height) {
    switch (format) {
    case InferenceBackend::FOURCC_BGRA:
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGR:
    case InferenceBackend::FOURCC_RGBA:
    case InferenceBackend::FOURCC_RGBX:
    case InferenceBackend::FOURCC_RGB:
        return std::unique_ptr<Renderer>(
            new gpu::draw::RendererRGB(converter, std::move(input_buffer_mapper), width, height));
    default:
        throw std::runtime_error("Unsupported format");
    }
}
