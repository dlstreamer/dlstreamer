/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/frame_info.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace dlstreamer {

ImageFormat avformat_to_image_format(int format) {
    switch (format) {
    case AV_PIX_FMT_RGB24:
        return ImageFormat::RGB;
    case AV_PIX_FMT_BGR24:
        return ImageFormat::BGR;
    case AV_PIX_FMT_RGBA:
        return ImageFormat::RGBX;
    case AV_PIX_FMT_BGRA:
        return ImageFormat::BGRX;
    case AV_PIX_FMT_RGB0:
        return ImageFormat::RGBX;
    case AV_PIX_FMT_BGR0:
        return ImageFormat::BGRX;
    case AV_PIX_FMT_VAAPI:
        return ImageFormat::NV12; // TODO
    }
    throw std::runtime_error("Unsupported AVPixelFormat: " + std::to_string(format));
}

FrameInfo avframe_to_info(AVFrame *frame) {
    FrameInfo info;
    info.media_type = MediaType::Image;
    auto format = avformat_to_image_format(frame->format);
    info.format = static_cast<Format>(format);
    size_t channels;
    if (format == ImageFormat::NV12) { // TODO two planes
        channels = 1;
    } else if (format == ImageFormat::RGB || format == ImageFormat::BGR) {
        channels = 3;
    } else if (format == ImageFormat::RGBX || format == ImageFormat::BGRX) {
        channels = 4;
    } else {
        throw std::runtime_error("Unsupported AVPixelFormat: " + std::to_string(frame->format));
    }
    std::vector<size_t> shape = {1, (size_t)frame->height, (size_t)frame->width, channels};
    std::vector<size_t> stride = {(size_t)frame->height * frame->linesize[0], (size_t)frame->linesize[0], channels, 1};
    info.tensors.push_back(TensorInfo(shape, DataType::UInt8, stride));
    return info;
}

} // namespace dlstreamer
