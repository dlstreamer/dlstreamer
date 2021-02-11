/*******************************************************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_images.h"

using namespace InferenceBackend;

namespace {

VASurfaceID CreateVASurface(VADisplay dpy, uint32_t width, uint32_t height, FourCC format,
                            int rt_format = VA_RT_FORMAT_YUV420) {
    VASurfaceAttrib surface_attrib;
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = format;

    VAConfigAttrib format_attrib;
    format_attrib.type = VAConfigAttribRTFormat;
    VA_CALL(vaGetConfigAttributes(dpy, VAProfileNone, VAEntrypointVideoProc, &format_attrib, 1));
    if (not(format_attrib.value & rt_format))
        throw std::invalid_argument("Unsupported runtime format for surface.");

    VASurfaceID va_surface_id;
    VA_CALL(vaCreateSurfaces(dpy, rt_format, width, height, &va_surface_id, 1, &surface_attrib, 1))
    return va_surface_id;
}

} // namespace

VaApiImage::VaApiImage() {
    image.va_surface_id = VA_INVALID_SURFACE;
    image.va_display = nullptr;
}

VaApiImage::VaApiImage(VaApiContext *context_, uint32_t width, uint32_t height, FourCC format, MemoryType memory_type) {
    context = context_;
    image.type = memory_type;
    image.width = width;
    image.height = height;
    image.format = format;
    image.va_display = context->Display();
    image.va_surface_id = CreateVASurface(image.va_display, width, height, format);
    image_map = std::unique_ptr<ImageMap>(ImageMap::Create(memory_type));
    completed = true;
}

VaApiImage::~VaApiImage() {
    if (image.type == MemoryType::VAAPI && image.va_surface_id != VA_INVALID_ID) {
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

VaApiImagePool::VaApiImagePool(VaApiContext *context_, size_t image_pool_size, ImageInfo info) {
    if (not context_)
        throw std::invalid_argument("VaApiContext is nullptr");
    for (size_t i = 0; i < image_pool_size; ++i) {
        _images.push_back(std::unique_ptr<VaApiImage>(
            new VaApiImage(context_, info.width, info.height, info.format, info.memory_type)));
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
    if (!image)
        throw std::runtime_error("Recived VA-API image is null");

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
