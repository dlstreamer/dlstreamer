/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_image_map.h"

#include "inference_backend/logger.h"

using namespace InferenceBackend;

ImageMap *ImageMap::Create(MemoryType type) {
    ImageMap *map = nullptr;
    switch (type) {
    case MemoryType::SYSTEM:
        map = new VaApiImageMap_SytemMemory();
        break;
    case MemoryType::VAAPI:
        map = new VaApiImageMap_VASurafce();
        break;
    default:
        throw std::invalid_argument("Unsupported format for ImageMap");
    }
    return map;
}

VaApiImageMap_SytemMemory::VaApiImageMap_SytemMemory() : va_display(nullptr), va_image(VAImage()) {
}

VaApiImageMap_SytemMemory::~VaApiImageMap_SytemMemory() {
    Unmap();
}

Image VaApiImageMap_SytemMemory::Map(const Image &image) {

    /* Throws in case of invalid VADisplay */
    auto dpy = VaDpyWrapper::fromHandle(image.va_display);
    va_display = dpy.raw();

    VA_CALL(dpy.drvVtable().vaDeriveImage(dpy.drvCtx(), image.va_surface_id, &va_image))

    void *surface_p = nullptr;
    VA_CALL(dpy.drvVtable().vaMapBuffer(dpy.drvCtx(), va_image.buf, &surface_p));

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

void VaApiImageMap_SytemMemory::Unmap() {
    if (va_display) {
        try {
            auto dpy = VaDpyWrapper::fromHandle(va_display);
            VA_CALL(dpy.drvVtable().vaUnmapBuffer(dpy.drvCtx(), va_image.buf));
            VA_CALL(dpy.drvVtable().vaDestroyImage(dpy.drvCtx(), va_image.image_id));
        } catch (const std::exception &e) {
            std::string error_message =
                std::string("VA buffer unmapping (destroying) failed with exception: ") + e.what();
            GVA_WARNING(error_message.c_str());
        }
    }
}

VaApiImageMap_VASurafce::VaApiImageMap_VASurafce() {
}

VaApiImageMap_VASurafce::~VaApiImageMap_VASurafce() {
    Unmap();
}

Image VaApiImageMap_VASurafce::Map(const Image &image) {
    return image;
}

void VaApiImageMap_VASurafce::Unmap() {
}