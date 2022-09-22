/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/base/memory_mapper.h>

#include "dpcpp_types.h"
#include "renderer.h"

#include <CL/sycl.hpp>

#include <gst/video/video-frame.h>

namespace gpu {
namespace draw {

class RendererGPU : public Renderer {
  protected:
    template <typename T>
    using gpu_unique_ptr = std::unique_ptr<T, std::function<void(T *)>>;

    struct TextStorage {
        uint8_t *map;
        cv::Size size;
        int baseline;
    };

    int image_width;
    int image_height;
    std::shared_ptr<sycl::queue> queue;
    std::map<std::string, TextStorage> text_storage;
    dlstreamer::MemoryMapperPtr buffer_mapper;

    gpu_unique_ptr<gpu::dpcpp::Rect> rectangles = nullptr;
    gpu_unique_ptr<gpu::dpcpp::Circle> circles = nullptr;
    gpu_unique_ptr<gpu::dpcpp::Line> lines = nullptr;
    gpu_unique_ptr<gpu::dpcpp::Text> texts = nullptr;
    uint32_t _rectangles_size = 0;
    uint32_t _circles_size = 0;
    uint32_t _lines_size = 0;
    uint32_t _texts_size = 0;

    gpu::dpcpp::Rect prepare_rectangle(render::Rect rect, int &max_side);
    std::vector<gpu::dpcpp::Text> prepare_text(const render::Text &drawing_text, int &max_width, int &max_height);
    gpu::dpcpp::Line prepare_line(const render::Line &line);

    dlstreamer::FramePtr buffer_map(dlstreamer::FramePtr buffer) override;
    void malloc_device_prims(render::Prim p, uint32_t size);

  public:
    RendererGPU(std::shared_ptr<ColorConverter> color_converter, dlstreamer::MemoryMapperPtr input_buffer_mapper,
                int image_width, int image_height);
    ~RendererGPU();
};

class RendererRGB : public RendererGPU {
  protected:
    void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<render::Prim> &prims) override;

  public:
    RendererRGB(std::shared_ptr<ColorConverter> color_converter, dlstreamer::MemoryMapperPtr input_buffer_mapper,
                int image_width, int image_height)
        : RendererGPU(color_converter, std::move(input_buffer_mapper), image_width, image_height) {
    }
};

} // namespace draw
} // namespace gpu
