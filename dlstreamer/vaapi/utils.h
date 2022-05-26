/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef ENABLE_VAAPI
#include "dlstreamer/buffer.h"
#include <va/va.h>

namespace dlstreamer {

static inline uint32_t format_to_vaapi(int format) {
    switch (format) {
    case FOURCC_BGRX:
        return VA_FOURCC_BGRA;
    case FOURCC_RGBX:
        return VA_FOURCC_RGBA;
    case FOURCC_BGRP:
        return VA_FOURCC_BGRP;
    case FOURCC_RGBP:
        return VA_FOURCC_RGBP;
    case FOURCC_NV12:
        return VA_FOURCC_NV12;
    case FOURCC_I420:
        return VA_FOURCC_I420;
    }
    throw std::runtime_error("format_to_vaapi: Unsupported format " + std::to_string(format));
}

static inline uint32_t vaapi_fourcc_to_rtformat(uint32_t fourcc) {
    switch (fourcc) {
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
        throw std::runtime_error("Unsupported fourcc format " + std::to_string(fourcc));
    }
}

} // namespace dlstreamer

#endif // ENABLE_VAAPI
