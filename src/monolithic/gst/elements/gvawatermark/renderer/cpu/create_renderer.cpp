/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "create_renderer.h"
#include "renderer_cpu.h"

std::unique_ptr<Renderer> create_cpu_renderer(dlstreamer::ImageFormat format, std::shared_ptr<ColorConverter> converter,
                                              dlstreamer::MemoryMapperPtr buffer_mapper) {

    switch (format) {
    case dlstreamer::ImageFormat::BGR:
    case dlstreamer::ImageFormat::RGB:
    case dlstreamer::ImageFormat::BGRX:
    case dlstreamer::ImageFormat::RGBX:
        return std::make_unique<RendererBGR>(converter, std::move(buffer_mapper));
    case dlstreamer::ImageFormat::NV12:
        return std::make_unique<RendererNV12>(converter, std::move(buffer_mapper));
    case dlstreamer::ImageFormat::I420:
        return std::make_unique<RendererI420>(converter, std::move(buffer_mapper));
    default:
        throw std::runtime_error("Unsupported format");
    }
}
