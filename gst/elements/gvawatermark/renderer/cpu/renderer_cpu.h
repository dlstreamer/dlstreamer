/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_mapper.h"
#include "renderer.h"

class RendererCPU : public Renderer {
  public:
    RendererCPU(std::shared_ptr<ColorConverter> color_converter, dlstreamer::BufferMapperPtr buffer_mapper)
        : Renderer(color_converter), _buffer_mapper(std::move(buffer_mapper)) {
    }
    ~RendererCPU();

  protected:
    dlstreamer::BufferPtr buffer_map(dlstreamer::BufferPtr buffer) override;

    dlstreamer::BufferMapperPtr _buffer_mapper;
};

class RendererYUV : public RendererCPU {
  public:
    RendererYUV(std::shared_ptr<ColorConverter> color_converter, dlstreamer::BufferMapperPtr buffer_mapper)
        : RendererCPU(color_converter, std::move(buffer_mapper)) {
    }

  protected:
    void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) override;

    virtual void draw_rectangle(std::vector<cv::Mat> &mats, gapidraw::Rect rect) = 0;
    virtual void draw_circle(std::vector<cv::Mat> &mats, gapidraw::Circle circle) = 0;
    virtual void draw_text(std::vector<cv::Mat> &mats, gapidraw::Text text) = 0;
    virtual void draw_line(std::vector<cv::Mat> &mats, gapidraw::Line line) = 0;

    void draw_rect_y_plane(cv::Mat &y, cv::Point2i pt1, cv::Point2i pt2, double color, int thick);
};

class RendererI420 : public RendererYUV {
  public:
    RendererI420(std::shared_ptr<ColorConverter> color_converter, dlstreamer::BufferMapperPtr buffer_mapper)
        : RendererYUV(color_converter, std::move(buffer_mapper)) {
    }

  protected:
    void draw_rectangle(std::vector<cv::Mat> &mats, gapidraw::Rect rect) override;
    void draw_circle(std::vector<cv::Mat> &mats, gapidraw::Circle circle) override;
    void draw_text(std::vector<cv::Mat> &mats, gapidraw::Text text) override;
    void draw_line(std::vector<cv::Mat> &mats, gapidraw::Line line) override;
};

class RendererNV12 : public RendererYUV {
  public:
    RendererNV12(std::shared_ptr<ColorConverter> color_converter, dlstreamer::BufferMapperPtr buffer_mapper)
        : RendererYUV(color_converter, std::move(buffer_mapper)) {
    }

  protected:
    void draw_rectangle(std::vector<cv::Mat> &mats, gapidraw::Rect rect) override;
    void draw_circle(std::vector<cv::Mat> &mats, gapidraw::Circle circle) override;
    void draw_text(std::vector<cv::Mat> &mats, gapidraw::Text text) override;
    void draw_line(std::vector<cv::Mat> &mats, gapidraw::Line line) override;
};

class RendererBGR : public RendererCPU {
  public:
    RendererBGR(std::shared_ptr<ColorConverter> color_converter, dlstreamer::BufferMapperPtr buffer_mapper)
        : RendererCPU(color_converter, std::move(buffer_mapper)) {
    }

  protected:
    void draw_backend(std::vector<cv::Mat> &image_planes, std::vector<gapidraw::Prim> &prims) override;
};
