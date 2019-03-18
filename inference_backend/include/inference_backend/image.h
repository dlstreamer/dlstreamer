/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <cstdint>

namespace InferenceBackend {

enum class MemoryType {
    ANY = 0,
    SYSTEM = 1,
    OPENCL = 2,
    VAAPI = 3,
};

enum FourCC {
    FOURCC_NV12 = 0x3231564E,
    FOURCC_BGRA = 0x41524742,
    FOURCC_BGRX = 0x58524742,
    FOURCC_BGRP = 0x50524742,
    FOURCC_BGR = 0x00524742,
    FOURCC_RGBA = 0x41424752,
    FOURCC_RGBX = 0x58424752,
    FOURCC_RGBP = 0x50424752,
    FOURCC_I420 = 0x30323449
};

struct Rectangle {
    int x;
    int y;
    int width;
    int height;
};

struct Image {
    MemoryType type;
    union {
        uint8_t *planes[4]; // if type==SYSTEM
        void *cl_mem;       // if type==OPENCL
        struct {            // if type==VAAPI
            void *va_display;
            unsigned int va_surface;
        };
    };
    int format; // FourCC
    int width;
    int height;
    int stride[4];
    Rectangle rect;
};

} // namespace InferenceBackend
