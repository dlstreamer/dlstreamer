/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "color_converter.h"

#include <dlstreamer/buffer.h>
#include <opencv2/gapi/render/render_types.hpp>

#include <memory>
#include <stdexcept>
#include <vector>

namespace gapidraw = cv::gapi::wip::draw;

class Renderer {
  public:
    void draw(dlstreamer::BufferPtr buffer, std::vector<gapidraw::Prim> prims);

    virtual ~Renderer() = default;

  protected:
    std::shared_ptr<ColorConverter> _color_converter;

    Renderer(std::shared_ptr<ColorConverter> color_converter) : _color_converter(color_converter) {
    }

    void convert_prims_color(std::vector<gapidraw::Prim> &prims);

    virtual void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) = 0;
    virtual dlstreamer::BufferPtr buffer_map(dlstreamer::BufferPtr buffer) = 0;

  private:
    static int FourccToOpenCVMatType(int fourcc);

    static std::vector<cv::Mat> convertBufferToCvMats(dlstreamer::Buffer &buffer);
};
