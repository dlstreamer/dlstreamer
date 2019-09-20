/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_images.h"

using namespace InferenceBackend;

VaApiImage::VaApiImage(VaApiContext *context_, int width, int height, int format) {
    context = context_;
    image.type = MemoryType::ANY;
    image.width = width;
    image.height = height;
    image.format = format;
    completed = true;
    image_map = std::unique_ptr<ImageMap>(ImageMap::Create());
}

VaApiImage::~VaApiImage() {
    if (image.type == MemoryType::VAAPI && image.va_surface_id && image.va_surface_id != VA_INVALID_ID) {
        try {
            VA_CALL(vaDestroySurfaces(image.va_display, (uint32_t *)&image.va_surface_id, 1));
        } catch (const std::exception &e) {
            std::string error_message = std::string("VA surface destroying failed with exception: ") + e.what();
            GVA_WARNING(error_message.c_str());
        }
    }
}

void VaApiImage::Unmap() {
    image_map->Unmap();
}

Image VaApiImage::Map() {
    return image_map->Map(image);
}

VaApiImagePool::VaApiImagePool(VaApiContext *context_, int image_pool_size, int width, int height, int format) {
    for (int i = 0; i < image_pool_size; ++i) {
        _images.push_back(std::unique_ptr<VaApiImage>(new VaApiImage(context_, width, height, format)));
    }
}

VaApiImage *VaApiImagePool::AcquireBuffer() {
    std::unique_lock<std::mutex> lock(_free_images_mutex);
    for (;;) {
        for (auto &image : _images) {
            if (image->completed) {
                image->completed = false;
                return image.get();
            }
        }
        _free_image_condition_variable.wait(lock);
    }
}

void VaApiImagePool::ReleaseBuffer(VaApiImage *image) {
    image->completed = true;
    _free_image_condition_variable.notify_one();
}

void VaApiImagePool::Flush() {
    std::unique_lock<std::mutex> lock(_free_images_mutex);
    for (auto &image : _images) {
        if (!image->completed)
            image->sync.wait();
    }
}
