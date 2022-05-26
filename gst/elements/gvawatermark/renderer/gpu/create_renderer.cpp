/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <dlstreamer/buffer_mapper.h>

#include "renderer_gpu.h"

#define VAAF_API __attribute__((__visibility__("default")))

VAAF_API std::unique_ptr<Renderer> create_renderer(dlstreamer::FourCC format, std::shared_ptr<ColorConverter> converter,
                                                   dlstreamer::BufferMapperPtr input_buffer_mapper, int width,
                                                   int height) {
    using dlstreamer::FourCC;
    switch (format) {
    case FourCC::FOURCC_BGRX:
    case FourCC::FOURCC_BGR:
    case FourCC::FOURCC_RGBX:
    case FourCC::FOURCC_RGB:
        return std::unique_ptr<Renderer>(
            new gpu::draw::RendererRGB(converter, std::move(input_buffer_mapper), width, height));
    default:
        throw std::runtime_error("Unsupported format");
    }
}
