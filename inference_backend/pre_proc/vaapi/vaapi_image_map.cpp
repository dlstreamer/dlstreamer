/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_image_map.h"
#include "vaapi_utils.h"

namespace InferenceBackend {

ImageMap *ImageMap::Create() {
    return new VAAPIImageMap();
}

VAAPIImageMap::VAAPIImageMap() : va_display(0), va_image({}) {
}

VAAPIImageMap::~VAAPIImageMap() {
    Unmap();
}

Image VAAPIImageMap::Map(const Image &image) {
    if (image.type != MemoryType::VAAPI)
        throw std::runtime_error("VAAPIImageMap supports only MemoryType::VAAPI");

    va_display = image.va_display;

    VAImageFormat va_format = {};
    if (image.format == VA_FOURCC_RGBP) {
        va_format = {.fourcc = (uint32_t)image.format,
                     .byte_order = VA_LSB_FIRST,
                     .bits_per_pixel = 24,
                     .depth = 24,
                     .red_mask = 0xff0000,
                     .green_mask = 0xff00,
                     .blue_mask = 0xff,
                     .alpha_mask = 0,
                     .va_reserved = {}};
    }

    // VA_CALL(vaSyncSurface(va_display, surface_id))

    if (va_format.fourcc &&
        vaCreateImage(va_display, &va_format, image.width, image.height, &va_image) == VA_STATUS_SUCCESS) {
        VA_CALL(vaGetImage(va_display, image.va_surface_id, 0, 0, image.width, image.height, va_image.image_id))
    } else {
        VA_CALL(vaDeriveImage(va_display, image.va_surface_id, &va_image))
    }

    void *surface_p = nullptr;
    VA_CALL(vaMapBuffer(va_display, va_image.buf, &surface_p))

    Image image_sys = {};
    image_sys.type = MemoryType::SYSTEM;
    image_sys.width = image.width;
    image_sys.height = image.height;
    image_sys.format = image.format;
    for (uint32_t i = 0; i < va_image.num_planes; i++) {
        image_sys.planes[i] = (uint8_t *)surface_p + va_image.offsets[i];
        image_sys.stride[i] = va_image.pitches[i];
    }

    return image_sys;
}

void VAAPIImageMap::Unmap() {
    if (va_display) {
        VA_CALL(vaUnmapBuffer(va_display, va_image.buf))
        VA_CALL(vaDestroyImage(va_display, va_image.image_id))
    }
}
} // namespace InferenceBackend
