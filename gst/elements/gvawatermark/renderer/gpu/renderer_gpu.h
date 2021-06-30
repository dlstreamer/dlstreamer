/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dpcpp_types.h"
#include "renderer.h"

#include <CL/sycl.hpp>

#include <gst/video/video-frame.h>

class BufferMapper;
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
    gpu_unique_ptr<gpu::dpcpp::MaskedPixel> mask;
    std::map<std::string, TextStorage> text_storage;
    std::shared_ptr<BufferMapper> buffer_mapper;
    sycl::event mask_clear_event;

    gpu_unique_ptr<gpu::dpcpp::Rect> rectangles = nullptr;
    gpu_unique_ptr<gpu::dpcpp::Circle> circles = nullptr;
    gpu_unique_ptr<gpu::dpcpp::Line> lines = nullptr;
    gpu_unique_ptr<gpu::dpcpp::Text> texts = nullptr;
    uint32_t _rectangles_size = 0;
    uint32_t _circles_size = 0;
    uint32_t _lines_size = 0;
    uint32_t _texts_size = 0;

    gpu::dpcpp::Rect prepare_rectangle(gapidraw::Rect rect, int &max_side);
    std::vector<gpu::dpcpp::Text> prepare_text(const gapidraw::Text &drawing_text, int &max_width, int &max_height);
    void draw_prims_on_mask(std::vector<cv::gapi::wip::draw::Prim> &prims);

    void buffer_map(GstBuffer *buffer, InferenceBackend::Image &image, BufferMapContext &map_context,
                    GstVideoInfo *info) override;
    void buffer_unmap(BufferMapContext &map_context) override;
    void malloc_device_prims(int index, uint32_t size);
    void clear_mask();

  public:
    RendererGPU(std::shared_ptr<ColorConverter> color_converter, InferenceBackend::MemoryType memory_type,
                int image_width, int image_height);
    ~RendererGPU();
};

class RendererNV12 : public RendererGPU {
  protected:
    void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<cv::gapi::wip::draw::Prim> &prims,
                      uint64_t drm_format_modifier) override;

  public:
    RendererNV12(std::shared_ptr<ColorConverter> color_converter, InferenceBackend::MemoryType memory_type,
                 int image_width, int image_height)
        : RendererGPU(color_converter, memory_type, image_width, image_height) {
    }
};

class RendererI420 : public RendererGPU {
  protected:
    void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<cv::gapi::wip::draw::Prim> &prims,
                      uint64_t drm_format_modifier) override;

  public:
    RendererI420(std::shared_ptr<ColorConverter> color_converter, InferenceBackend::MemoryType memory_type,
                 int image_width, int image_height)
        : RendererGPU(color_converter, memory_type, image_width, image_height) {
    }
};

class RendererBGR : public RendererGPU {
  protected:
    void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<cv::gapi::wip::draw::Prim> &prims,
                      uint64_t drm_format_modifier) override;

  public:
    RendererBGR(std::shared_ptr<ColorConverter> color_converter, InferenceBackend::MemoryType memory_type,
                int image_width, int image_height)
        : RendererGPU(color_converter, memory_type, image_width, image_height) {
    }
};

} // namespace draw
} // namespace gpu
