/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <dlstreamer/base/memory_mapper.h>

#include "renderer_gpu.h"

#define VAAF_API __attribute__((__visibility__("default")))

extern "C" {

VAAF_API Renderer *create_renderer(dlstreamer::ImageFormat format, std::shared_ptr<ColorConverter> converter,
                                   dlstreamer::MemoryMapperPtr input_buffer_mapper, int width, int height) {
    switch (format) {
    case dlstreamer::ImageFormat::BGRX:
    case dlstreamer::ImageFormat::BGR:
    case dlstreamer::ImageFormat::RGBX:
    case dlstreamer::ImageFormat::RGB:
        return new gpu::draw::RendererRGB(converter, std::move(input_buffer_mapper), width, height);
    default:
        throw std::runtime_error("Unsupported format");
    }
}
}
