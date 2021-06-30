/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <type_traits>

namespace {
template <int a, int b, int c, int d>
struct fourcc {
    enum { code = (a) | (b << 8) | (c << 16) | (d << 24) };
};

} // namespace

namespace InferenceBackend {

enum class MemoryType { ANY = 0, SYSTEM = 1, DMA_BUFFER = 2, VAAPI = 3, USM_DEVICE_POINTER = 4 };

enum FourCC {
    FOURCC_RGBP_F32 = 0x07282024,
    FOURCC_NV12 = fourcc<'N', 'V', '1', '2'>::code,
    FOURCC_BGRA = fourcc<'B', 'G', 'R', 'A'>::code,
    FOURCC_BGRX = fourcc<'B', 'G', 'R', 'X'>::code,
    FOURCC_BGRP = fourcc<'B', 'G', 'R', 'P'>::code,
    FOURCC_BGR = fourcc<'B', 'G', 'R', ' '>::code,
    FOURCC_RGBA = fourcc<'R', 'G', 'B', 'A'>::code,
    FOURCC_RGBX = fourcc<'R', 'G', 'B', 'X'>::code,
    FOURCC_RGB = fourcc<'R', 'G', 'B', ' '>::code,
    FOURCC_RGBP = fourcc<'R', 'G', 'B', 'P'>::code,
    FOURCC_I420 = fourcc<'I', '4', '2', '0'>::code,
    FOURCC_YUV = fourcc<'Y', 'U', 'V', ' '>::code
};

template <typename T>
struct Rectangle {
    static_assert(std::is_floating_point<T>::value or std::is_integral<T>::value,
                  "struct Rectangle can only be instantiated with numeric type types");

    T x = 0;
    T y = 0;
    T width = 0;
    T height = 0;

    Rectangle() = default;
    Rectangle(T x, T y, T width, T height) : x(x), y(y), width(width), height(height) {
    }
};

struct Image {
    MemoryType type = MemoryType::ANY;
    static const uint32_t MAX_PLANES_NUMBER = 4;

    union {
        uint8_t *planes[MAX_PLANES_NUMBER]; // if type==SYSTEM
        struct {                            // if type==VAAPI
            uint32_t va_surface_id;
            void *va_display;
        };
    };
    int dma_fd = -1; // if type==DMA_BUFFER or VPUX device is used

    int format = 0; // FourCC
    uint64_t drm_format_modifier = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t size = 0;
    uint32_t stride[MAX_PLANES_NUMBER] = {0, 0, 0, 0};
    uint32_t offsets[MAX_PLANES_NUMBER] = {0, 0, 0, 0};
    Rectangle<uint32_t> rect;

    Image() = default;
};

// Map DMA/VAAPI image into system memory
class ImageMap {
  public:
    virtual Image Map(const Image &image) = 0;
    virtual void Unmap() = 0;

    static ImageMap *Create(MemoryType type);
    virtual ~ImageMap() = default;
};

} // namespace InferenceBackend
