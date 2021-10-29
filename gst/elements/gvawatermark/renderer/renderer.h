/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "color_converter.h"

#include "inference_backend/image.h"

#include <gst/video/video-info.h>

#include <opencv2/gapi/render/render_types.hpp>

#include <memory>
#include <stdexcept>
#include <vector>

namespace gapidraw = cv::gapi::wip::draw;

class Renderer {
  public:
    void draw(GstBuffer *buffer, std::vector<gapidraw::Prim> prims);

    virtual ~Renderer() = default;

  protected:
    std::shared_ptr<ColorConverter> _color_converter;
    InferenceBackend::MemoryType _memory_type;

    Renderer(std::shared_ptr<ColorConverter> color_converter, InferenceBackend::MemoryType memory_type)
        : _color_converter(color_converter), _memory_type(memory_type) {
    }

    void convert_prims_color(std::vector<gapidraw::Prim> &prims);

    virtual void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) = 0;
    virtual void buffer_map(GstBuffer *buffer, InferenceBackend::Image &image) = 0;
    virtual void buffer_unmap(InferenceBackend::Image &image) = 0;

  private:
    static int FourccToOpenCVMatType(int fourcc);

    static std::vector<cv::Mat> convertImageToMat(const InferenceBackend::Image &image);
};
