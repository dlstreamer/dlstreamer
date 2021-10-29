/*******************************************************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_images.h"

using namespace InferenceBackend;

namespace {

VASurfaceID CreateVASurface(VaDpyWrapper display, uint32_t width, uint32_t height, int pixel_format, int rt_format) {
    VASurfaceAttrib surface_attrib;
    surface_attrib.type = VASurfaceAttribPixelFormat;
    surface_attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;
    surface_attrib.value.type = VAGenericValueTypeInteger;
    surface_attrib.value.value.i = pixel_format;

    VASurfaceID va_surface_id;
    VA_CALL(display.drvVtable().vaCreateSurfaces2(display.drvCtx(), rt_format, width, height, &va_surface_id, 1,
                                                  &surface_attrib, 1))
    return va_surface_id;
}

struct Format {
    uint32_t va_fourcc;
    InferenceBackend::FourCC ib_fourcc;
};

// This structure contains formats supported by software processing, in order of priority.
constexpr Format possible_formats[] = {{VA_FOURCC_BGRA, InferenceBackend::FourCC::FOURCC_BGRA},
                                       {VA_FOURCC_BGRX, InferenceBackend::FourCC::FOURCC_BGRX},
                                       {VA_FOURCC_I420, InferenceBackend::FourCC::FOURCC_I420},
                                       {VA_FOURCC_NV12, InferenceBackend::FourCC::FOURCC_NV12}};

std::string FourccName(int code) {
    const char c1 = (code & (0x000000ff << 24)) >> 24;
    const char c2 = (code & (0x000000ff << 16)) >> 16;
    const char c3 = (code & (0x000000ff << 8)) >> 8;
    const char c4 = code & 0x000000ff;

    return {c4, c3, c2, c1};
}

} // namespace

VaApiImage::VaApiImage() {
    image.va_surface_id = VA_INVALID_SURFACE;
    image.va_display = nullptr;
}

VaApiImage::VaApiImage(VaApiContext *context_, uint32_t width, uint32_t height, int pixel_format,
                       MemoryType memory_type, uint32_t scaling_flgs /*= VA_FILTER_SCALING_DEFAULT*/) {
    if (!context_)
        throw std::invalid_argument("Invalid Vaapi context object");

    context = context_;
    image.type = memory_type;
    image.width = width;
    image.height = height;
    image.format = pixel_format;
    image.va_display = context->DisplayRaw();
    image.va_surface_id = CreateVASurface(context->Display(), width, height, pixel_format, context_->RTFormat());
    image_map = std::unique_ptr<ImageMap>(ImageMap::Create(memory_type));
    completed = true;
    scaling_flags = scaling_flgs;
}

VaApiImage::~VaApiImage() {
    if (image.type == MemoryType::VAAPI && image.va_surface_id != VA_INVALID_ID) {
        try {
            auto dpy = VaDpyWrapper::fromHandle(image.va_display);
            VA_CALL(dpy.drvVtable().vaDestroySurfaces(dpy.drvCtx(), (uint32_t *)&image.va_surface_id, 1));
        } catch (const std::exception &e) {
            GVA_WARNING("VA surface destroying failed: %s", e.what());
        }
    }
}

void VaApiImage::Unmap() {
    image_map->Unmap();
}

Image VaApiImage::Map() {
    return image_map->Map(image);
}

VaApiImagePool::VaApiImagePool(VaApiContext *context, SizeParams size_params, ImageInfo info) {
    if (!context)
        throw std::invalid_argument("VaApiContext is nullptr");

    if (size_params.size() == 0)
        throw std::invalid_argument("size_params can't be zero");

    if (!context->IsPixelFormatSupported(info.format)) {
        std::string msg = "Unsupported requested pixel format " + FourccName(info.format) + ". ";
        switch (info.memory_type) {
        case InferenceBackend::MemoryType::SYSTEM: {
            // In the case when the system memory is requested, we can choose the supported format and do software color
            // conversion after.
            bool is_set = false;
            for (auto format : possible_formats)
                if (context->IsPixelFormatSupported(format.va_fourcc)) {
                    msg += "Using a supported format " + FourccName(format.va_fourcc) + ".";
                    info.format = format.ib_fourcc;
                    is_set = true;
                    break;
                }
            if (not is_set)
                throw std::runtime_error(msg + "Could not set the other pixel format, none are supported.");
            else
                GVA_WARNING("%s", msg.c_str());
            break;
        }
        // In the case when the vaapi memory is requested, we cannot do software color conversion after.
        case InferenceBackend::MemoryType::VAAPI:
            throw std::runtime_error("Could not set the pixel format for vaapi memory. " + msg);
        default:
            throw std::runtime_error(msg + "Memory type is not supported to select an alternative pixel format.");
        }
    }

    GVA_INFO("VA-API image pool size: default=%u, fast=%u", size_params.num_default, size_params.num_fast);

    _images.reserve(size_params.size());
    for (size_t i = 0; i < size_params.size(); i++) {
        const uint32_t scaling_method = i < size_params.num_fast ? VA_FILTER_SCALING_FAST : VA_FILTER_SCALING_DEFAULT;
        _images.push_back(std::unique_ptr<VaApiImage>(
            new VaApiImage(context, info.width, info.height, info.format, info.memory_type, scaling_method)));
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
        throw std::runtime_error("Received VA-API image is null");

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
