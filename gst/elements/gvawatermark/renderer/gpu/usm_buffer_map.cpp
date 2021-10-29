/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "usm_buffer_map.h"

#include <gst/allocators/allocators.h>

#include <va/va_backend.h>
#include <va/va_drmcommon.h>

#include <unistd.h>

#include <iostream>

namespace {
uint32_t GetPlanesCount(int fourcc) {
    using namespace InferenceBackend;
    switch (fourcc) {
    case FOURCC_BGRA:
    case FOURCC_BGRX:
    case FOURCC_BGR:
    case FOURCC_RGBA:
    case FOURCC_RGBX:
        return 1;
    case FOURCC_NV12:
        return 2;
    case FOURCC_BGRP:
    case FOURCC_RGBP:
    case FOURCC_I420:
        return 3;
    }

    return 0;
}
} // namespace

UsmBufferMapper::UsmBufferMapper(std::shared_ptr<sycl::queue> queue, std::unique_ptr<BufferMapper> input_buffer_mapper)
    : _queue(queue), _input_mapper(std::move(input_buffer_mapper)) {
    if (!_queue)
        throw std::invalid_argument("queue is null");
    if (!_input_mapper)
        throw std::invalid_argument("input_buffer_mapper is null");

    auto in_mem_type = _input_mapper->memoryType();
    if (in_mem_type != InferenceBackend::MemoryType::DMA_BUFFER && in_mem_type != InferenceBackend::MemoryType::VAAPI) {
        throw std::invalid_argument("only VAAPI and DMA buffer are supported for input_buffer_mapper");
    }
};

InferenceBackend::MemoryType UsmBufferMapper::memoryType() const {
    return InferenceBackend::MemoryType::USM_DEVICE_POINTER;
}

InferenceBackend::Image UsmBufferMapper::map(GstBuffer *buffer, GstMapFlags flags) {
    assert(_queue);

    size_t dma_size = 0;
    auto image = mapToDmaImg(buffer, flags, &dma_size);

    image.map_context = getDeviceMemPointer(*_queue, image.dma_fd, dma_size);
    assert(image.map_context);

    const size_t n_planes = GetPlanesCount(image.format);
    for (size_t i = 0; i < n_planes; ++i) {
        image.planes[i] = static_cast<uint8_t *>(image.map_context) + image.offsets[i];
    }
    image.type = memoryType();

    return image;
}

void UsmBufferMapper::unmap(InferenceBackend::Image &image) {
    if (!image.map_context)
        return;

    assert((image.map_context == image.planes[0] - image.offsets[0]) && "Invalid image provided for unmap operation");

    ze_context_handle_t ze_context = _queue->get_context().get_native<sycl::backend::level_zero>();
    zeMemFree(ze_context, image.map_context);
    close(image.dma_fd);

    image.map_context = nullptr;
    image.dma_fd = -1;
}

InferenceBackend::Image UsmBufferMapper::mapToDmaImg(GstBuffer *buffer, GstMapFlags flags, size_t *out_dma_size) {
    using namespace InferenceBackend;
    assert(_input_mapper);

    // Init DMA size if provided
    if (out_dma_size)
        *out_dma_size = 1024;

    auto in_image = _input_mapper->map(buffer, flags);
    assert((in_image.type == MemoryType::VAAPI || in_image.type == MemoryType::DMA_BUFFER) &&
           "VAAPI or DMA buffer is expected as mapped memory type");

    Image image = in_image;

    if (in_image.type == MemoryType::VAAPI) {
        // VAAPI: it's needed to export VASurface to obtain DMA FD.
        image = convertSurfaceImgToDmaImg(in_image, out_dma_size);
    } else {
        // DMA: WA for issue in Level Zero when zeMemFree called FD that was passed to export external memory will be
        // closed but it shouldn`t.
        // This can be removed once fixed in Level Zero.
        // However, in this case, the unmap method needs to be updated to not close the FD for DMA case.
        image.dma_fd = dup(image.dma_fd);
    }

    // Initial mapped image no longer needed we have everything we need.
    _input_mapper->unmap(in_image);

    return image;
}

InferenceBackend::Image UsmBufferMapper::convertSurfaceImgToDmaImg(const InferenceBackend::Image &image,
                                                                   size_t *out_dma_size) {
    assert(image.type == InferenceBackend::MemoryType::VAAPI);
    assert(image.va_display);

    auto driver_context = reinterpret_cast<VADisplayContextP>(image.va_display)->pDriverContext;
    if (!driver_context)
        throw std::runtime_error("VA driver context is null");
    auto vtable = driver_context->vtable;

    VADRMPRIMESurfaceDescriptor prime_desc{};
    vtable->vaExportSurfaceHandle(driver_context, image.va_surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                  VA_EXPORT_SURFACE_READ_WRITE, &prime_desc);

    // Copy the image structure and update it with DMA-related stuff
    auto res_image = image;

    res_image.type = InferenceBackend::MemoryType::DMA_BUFFER;
    res_image.dma_fd = prime_desc.objects[0].fd;
    if (out_dma_size)
        *out_dma_size = prime_desc.objects[0].size;
    res_image.drm_format_modifier = prime_desc.objects[0].drm_format_modifier; // non-zero if tiled (non-linear) mem

    // Update stride and offset for each plane
    uint32_t plane_num = 0;
    for (uint32_t i = 0; i < prime_desc.num_layers; i++) {
        const auto layer = &prime_desc.layers[i];
        for (uint32_t j = 0; j < layer->num_planes; j++) {
            if (plane_num >= InferenceBackend::Image::MAX_PLANES_NUMBER)
                break;

            res_image.stride[plane_num] = layer->pitch[j];
            res_image.offsets[plane_num] = layer->offset[j];
            plane_num++;
        }
    }

    return res_image;
}

void *UsmBufferMapper::getDeviceMemPointer(sycl::queue &queue, int dma_fd, size_t dma_size) {
    ze_context_handle_t ze_context = queue.get_context().get_native<sycl::backend::level_zero>();
    ze_device_handle_t ze_device = queue.get_device().get_native<sycl::backend::level_zero>();

    ze_external_memory_import_fd_t import_fd = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD,
                                                nullptr, // pNext
                                                ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF, dma_fd};
    ze_device_mem_alloc_desc_t alloc_desc = {};
    alloc_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    alloc_desc.pNext = &import_fd;

    void *ptr = nullptr;
    ze_result_t ze_res = zeMemAllocDevice(ze_context, &alloc_desc, dma_size, 1, ze_device, &ptr);
    if (ze_res != ZE_RESULT_SUCCESS) {
        throw std::runtime_error("Failed to get USM pointer:" + std::to_string(ze_res));
    }

    return ptr;
}
