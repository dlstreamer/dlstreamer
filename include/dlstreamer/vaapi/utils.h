/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/frame.h"
#include <va/va.h>

namespace dlstreamer {

static inline uint32_t video_format_to_vaapi(ImageFormat format) {
    switch (format) {
    case ImageFormat::RGB:
    case ImageFormat::BGR:
        throw std::runtime_error("Unsupported format " + image_format_to_string(format));
    case ImageFormat::BGRX:
        return VA_FOURCC_BGRA;
    case ImageFormat::RGBX:
        return VA_FOURCC_RGBA;
    case ImageFormat::BGRP:
        return VA_FOURCC_BGRP;
    case ImageFormat::RGBP:
        return VA_FOURCC_RGBP;
    case ImageFormat::NV12:
        return VA_FOURCC_NV12;
    case ImageFormat::I420:
        return VA_FOURCC_I420;
    }
    throw std::runtime_error("format_to_vaapi: Unsupported format " + image_format_to_string(format));
}

static inline uint32_t vaapi_video_format_to_rtformat(uint32_t video_format) {
    switch (video_format) {
    case VA_FOURCC_I420:
    case VA_FOURCC_NV12:
    case VA_FOURCC_YV12:
        return VA_RT_FORMAT_YUV420;
    // case VA_FOURCC_P010:
    //     return VA_RT_FORMAT_YUV420_10;
    case VA_FOURCC_YUY2:
    case VA_FOURCC_UYVY:
        return VA_RT_FORMAT_YUV422;
    case VA_FOURCC_AYUV:
        return VA_RT_FORMAT_YUV444;
    case VA_FOURCC_RGBP:
        return VA_RT_FORMAT_RGBP;
    case VA_FOURCC_ARGB:
    case VA_FOURCC_ABGR:
    case VA_FOURCC_RGBA:
    case VA_FOURCC_BGRA:
        return VA_RT_FORMAT_RGB32;
    case VA_FOURCC_Y800:
        return VA_RT_FORMAT_YUV400;
    default:
        throw std::runtime_error("Unsupported video_format format " + std::to_string(video_format));
    }
}

} // namespace dlstreamer
