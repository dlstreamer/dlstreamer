/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "create_renderer.h"
#include "renderer_cpu.h"

std::unique_ptr<Renderer> create_cpu_renderer(dlstreamer::FourCC format, std::shared_ptr<ColorConverter> converter,
                                              dlstreamer::BufferMapperPtr buffer_mapper) {

    switch (format) {
    case dlstreamer::FOURCC_BGR:
    case dlstreamer::FOURCC_RGB:
    case dlstreamer::FOURCC_BGRX:
    case dlstreamer::FOURCC_RGBX:
        return std::unique_ptr<Renderer>(new RendererBGR(converter, std::move(buffer_mapper)));
    case dlstreamer::FOURCC_NV12:
        return std::unique_ptr<Renderer>(new RendererNV12(converter, std::move(buffer_mapper)));
    case dlstreamer::FOURCC_I420:
        return std::unique_ptr<Renderer>(new RendererI420(converter, std::move(buffer_mapper)));
    default:
        throw std::runtime_error("Unsupported format");
    }
}
