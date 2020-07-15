/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_image_map.h"
#include "vaapi_utils.h"

#include "inference_backend/logger.h"

namespace InferenceBackend {

ImageMap *ImageMap::Create() {
    return new VaApiImageMap();
}

VaApiImageMap::VaApiImageMap() : va_display(nullptr), va_image(VAImage()) {
}

VaApiImageMap::~VaApiImageMap() {
    Unmap();
}

Image VaApiImageMap::Map(const Image &image) {
    if (image.type != MemoryType::VAAPI)
        throw std::runtime_error("VAAPIImageMap supports only MemoryType::VAAPI");

    va_display = image.va_display;

    VA_CALL(vaDeriveImage(va_display, image.va_surface_id, &va_image))

    void *surface_p = nullptr;
    VA_CALL(vaMapBuffer(va_display, va_image.buf, &surface_p))

    Image image_sys = Image();
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

void VaApiImageMap::Unmap() {
    if (va_display) {
        try {
            VA_CALL(vaUnmapBuffer(va_display, va_image.buf))
            VA_CALL(vaDestroyImage(va_display, va_image.image_id))
        } catch (const std::exception &e) {
            std::string error_message =
                std::string("VA buffer unmapping (destroying) failed with exception: ") + e.what();
            GVA_WARNING(error_message.c_str());
        }
    }
}
} // namespace InferenceBackend
