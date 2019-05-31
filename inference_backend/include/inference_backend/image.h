/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <cstdint>

namespace InferenceBackend {

enum class MemoryType { ANY = 0, SYSTEM = 1, OPENCL = 2 };

enum FourCC {
    FOURCC_NV12 = 0x3231564E,
    FOURCC_BGRA = 0x41524742,
    FOURCC_BGRX = 0x58524742,
    FOURCC_BGRP = 0x50524742,
    FOURCC_BGR = 0x00524742,
    FOURCC_RGBA = 0x41424752,
    FOURCC_RGBX = 0x58424752,
    FOURCC_RGBP = 0x50424752,
    FOURCC_RGBP_F32 = 0x07282024,
    FOURCC_I420 = 0x30323449,
};

struct Rectangle {
    int x;
    int y;
    int width;
    int height;
};

struct Image {
    MemoryType type;
    static const unsigned MAX_PLANES_NUMBER = 4;
    union {
        uint8_t *planes[MAX_PLANES_NUMBER]; // if type==SYSTEM
        void *cl_mem;                       // if type==OPENCL
    };
    int format; // FourCC
    int width;
    int height;
    int stride[MAX_PLANES_NUMBER];
    Rectangle rect;
};

} // namespace InferenceBackend
