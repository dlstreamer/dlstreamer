/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "usm_buffer_map.h"
#include "gva_buffer_map.h"

#include <gst/allocators/allocators.h>

#include <va/va_backend.h>
#include <va/va_drmcommon.h>

#include <unistd.h>

#include <iostream>

namespace {

void CheckVaStatus(VAStatus status, const std::string &throw_message) {
    if (status != VA_STATUS_SUCCESS)
        throw std::runtime_error(throw_message);
}

} // namespace

struct ZeDeviceMemContext {
    void *ze_mem_device_buf_ptr = nullptr;

    VADriverContextP driver_context = nullptr;
    VAImage va_image = {};
};

UsmBufferMapper::UsmBufferMapper(std::shared_ptr<sycl::queue> queue) : _queue(queue) {
    if (!_queue) {
        throw std::logic_error("sycl::queue required for MemoryType::USM_DEVICE_POINTER");
    }
    _map_context = std::unique_ptr<ZeDeviceMemContext>(new ZeDeviceMemContext);
};

InferenceBackend::Image UsmBufferMapper::map(GstBuffer *buffer, GstVideoInfo *info, GstMapFlags) {
    fill_image_with_video_info(info, _image);
    assert(_queue != nullptr);

    size_t n_planes = GST_VIDEO_INFO_N_PLANES(info);
    mapGstBuffer(buffer, _image, _map_context.get());

    size_t dma_size = 1024;
    _map_context->ze_mem_device_buf_ptr = allocateZeMem(_queue, _image.dma_fd, dma_size);
    for (size_t i = 0; i < n_planes; ++i) {
        _image.planes[i] = (uint8_t *)_map_context->ze_mem_device_buf_ptr + _image.offsets[i];
    }

    _image.type = InferenceBackend::MemoryType::USM_DEVICE_POINTER;
    return _image;
}

void UsmBufferMapper::unmap() {
    if (_map_context->ze_mem_device_buf_ptr) {
        ze_context_handle_t ze_context = _queue->get_context().get_native<sycl::backend::level_zero>();
        // WA for issue in Level Zero when zeMemFree called FD that was passed to export external memory will
        // be closed but shouldn`t.
        int tmp_fd = dup(_image.dma_fd); // TODO: Remove when fixed in LZ
        zeMemFree(ze_context, _map_context->ze_mem_device_buf_ptr);
        dup2(tmp_fd, _image.dma_fd); // TODO: Remove when fixed in LZ
        close(tmp_fd);               // TODO: Remove when fixed in LZ

        auto driver_context = _map_context->driver_context;
        auto vtable = driver_context->vtable;
        VAStatus status;
        status = vtable->vaReleaseBufferHandle(driver_context, _map_context->va_image.buf);
        status |= vtable->vaDestroyImage(driver_context, _map_context->va_image.image_id);
        CheckVaStatus(status, "Failed destoy vaimage or release buffer handle.");

        memset(_map_context.get(), 0, sizeof(ZeDeviceMemContext));
        memset(&_image, 0, sizeof(InferenceBackend::Image));
    }
}

void UsmBufferMapper::mapGstBuffer(GstBuffer *buffer, InferenceBackend::Image &image, ZeDeviceMemContext *map_context) {
    GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
    if (!mem) {
        throw std::runtime_error("Failed to get GstBuffer memory");
    }

    if (gst_is_dmabuf_memory(mem)) { // memory:DMABuf
        image.dma_fd = gst_dmabuf_memory_get_fd(mem);
    } else { // memory:VASurface
        // query VASurfaceID, then get DRM fd from VASurfaceID
        void *va_display = gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VADisplay"));
        int va_surface_id =
            (uint64_t)gst_mini_object_get_qdata(&buffer->mini_object, g_quark_from_static_string("VASurfaceID"));
        if (!va_display) {
            throw std::runtime_error("Failed to get VADisplay");
        }

        auto driver_context = reinterpret_cast<VADisplayContextP>(va_display)->pDriverContext;
        if (!driver_context)
            throw std::runtime_error("Driver context is null");
        auto vtable = driver_context->vtable;

        VAStatus status;
        // Export drm prime decription to get information about tiling - drm_format_modifier.
        VADRMPRIMESurfaceDescriptor drm_desc = {};
        status = vtable->vaExportSurfaceHandle(driver_context, va_surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                               VA_EXPORT_SURFACE_READ_WRITE, &drm_desc);
        CheckVaStatus(status, "Could not export DRM PRIME surface handle.");

        if (drm_desc.num_objects != 1)
            throw std::runtime_error("Drm descriptor has unsupported number of objects.");
        image.drm_format_modifier = drm_desc.objects[0].drm_format_modifier; // non-zero if tiled (non-linear) mem
        close(drm_desc.objects[0].fd);

        map_context->driver_context = driver_context;

        status = vtable->vaDeriveImage(driver_context, va_surface_id, &(map_context->va_image));
        CheckVaStatus(status, "Could not derive vaimage from surface.");

        VAImage &va_image = map_context->va_image;
        VABufferInfo buffer_info = {};
        status = vtable->vaAcquireBufferHandle(driver_context, va_image.buf, &buffer_info);
        CheckVaStatus(status, "Could not acquire vaimage buffer handle.");
        image.dma_fd = buffer_info.handle;

        image.type = InferenceBackend::MemoryType::DMA_BUFFER;

        image.height = va_image.height;
        image.width = va_image.width;
        for (uint32_t i = 0; i < va_image.num_planes; i++) {
            image.stride[i] = va_image.pitches[i];
            image.offsets[i] = va_image.offsets[i];
        }
        image.format = va_image.format.fourcc;
    }
}

void *UsmBufferMapper::allocateZeMem(std::shared_ptr<sycl::queue> queue, int dma_fd, size_t dma_size) {

    ze_context_handle_t ze_context = queue->get_context().get_native<sycl::backend::level_zero>();
    ze_device_handle_t ze_device = queue->get_device().get_native<sycl::backend::level_zero>();

    ze_external_memory_import_fd_t import_fd = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD, nullptr,
                                                ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF, dma_fd};
    ze_device_mem_alloc_desc_t alloc_desc = {};
    alloc_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    alloc_desc.pNext = &import_fd;
    void *ptr = nullptr;
    ze_result_t ze_res = zeMemAllocDevice(ze_context, &alloc_desc, dma_size, 1, ze_device, &ptr);

    if (ze_res != ZE_RESULT_SUCCESS) {
        throw std::runtime_error("Failed to allocate ze device memory.");
    }

    return ptr;
}

UsmBufferMapper::~UsmBufferMapper() {
}