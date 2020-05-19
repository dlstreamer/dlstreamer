/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <cstdint>

namespace InferenceBackend {

enum class MemoryType {
    ANY = 0,
    SYSTEM = 1,
    DMA_BUFFER = 2,
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
    FOURCC_RGBP_F32 = 0x07282024,
    FOURCC_I420 = 0x30323449,
};

struct Rectangle {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
};

struct Image {
    MemoryType type;
    static const uint32_t MAX_PLANES_NUMBER = 4;
    union {
        uint8_t *planes[MAX_PLANES_NUMBER]; // if type==SYSTEM
        int dma_fd;                         // if type==DMA_BUFFER
        struct {
            uint32_t va_surface_id;
            void *va_display;
        };
    };
    int format; // FourCC
    uint32_t width;
    uint32_t height;
    uint32_t stride[MAX_PLANES_NUMBER];
    Rectangle rect;
};

// Map DMA/VAAPI image into system memory
class ImageMap {
  public:
    virtual Image Map(const Image &image) = 0;
    virtual void Unmap() = 0;

    static ImageMap *Create();
    virtual ~ImageMap() = default;
};

} // namespace InferenceBackend
