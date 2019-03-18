/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "va_image_locker.h"
#include "vaapi_utils.h"

VAImageLocker::VAImageLocker() : va_display(0), va_image({}), surface_p(nullptr) {
}

VAImageLocker::~VAImageLocker() {
    Unmap();
}

VAStatus VAImageLocker::Map(VADisplay va_display, VASurfaceID surface_id, VAImageFormat *va_format, int width,
                            int height) {
    this->va_display = va_display;
    // va_image.offsets[0]

    if (va_format && va_format->fourcc &&
        vaCreateImage(va_display, va_format, width, height, &va_image) == VA_STATUS_SUCCESS) {
        VA_CALL(vaGetImage(va_display, surface_id, 0, 0, width, height, va_image.image_id));
    } else {
        VA_CALL(vaDeriveImage(va_display, surface_id, &va_image));
    }

    VA_CALL(vaMapBuffer(va_display, va_image.buf, &surface_p));

    return VA_STATUS_SUCCESS;
}

void VAImageLocker::GetImageBuffer(uint8_t **planes, int *stride) {
    for (uint32_t i = 0; i < va_image.num_planes; i++) {
        planes[i] = (uint8_t *)surface_p + va_image.offsets[i];
        stride[i] = va_image.pitches[i];
    }
}

VAStatus VAImageLocker::Unmap() {
    if (surface_p) {
        VA_CALL(vaUnmapBuffer(va_display, va_image.buf));
        VA_CALL(vaDestroyImage(va_display, va_image.image_id));
        surface_p = nullptr;
    }
    return VA_STATUS_SUCCESS;
}
