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

struct usm_ptr_context_t {
    ze_context_handle_t ze_context;
    void *ptr;
    InferenceBackend::Image image;
};

UsmBufferMapper::UsmBufferMapper(std::shared_ptr<sycl::queue> queue) : _queue(queue) {
    if (!_queue) {
        throw std::logic_error("sycl::queue required for MemoryType::USM_DEVICE_POINTER");
    }
};

InferenceBackend::Image UsmBufferMapper::map(GstBuffer *buffer, GstVideoInfo *info, GstMapFlags) {
    fill_image_with_video_info(info, _image);
    assert(_queue != nullptr);

    size_t n_planes = GST_VIDEO_INFO_N_PLANES(info);
    void *ptr = get_device_pointer(_queue, buffer, &_image);
    for (size_t i = 0; i < n_planes; ++i) {
        _image.planes[i] = (uint8_t *)ptr + _image.offsets[i];
    }

    ze_context_handle_t ze_context = _queue->get_context().get_native<sycl::backend::level_zero>();

    // USM pointer will be closed in UsmBufferMapper::unmap
    _ze_context = ze_context;
    _usm_ptr = ptr;

    _image.type = InferenceBackend::MemoryType::USM_DEVICE_POINTER;
    return _image;
}

void UsmBufferMapper::unmap() {
    if (_ze_context && _usm_ptr) {
        // WA for issue in Level Zero when zeMemFree called FD that was passed to export external memory will
        // be closed but shouldn`t.
        int tmp_fd = dup(_dma_fd); // TODO: Remove when fixed in LZ
        zeMemFree((ze_context_handle_t)_ze_context, _usm_ptr);
        dup2(tmp_fd, _dma_fd); // TODO: Remove when fixed in LZ
        close(tmp_fd);         // TODO: Remove when fixed in LZ
    }
}

void *UsmBufferMapper::get_device_pointer(std::shared_ptr<sycl::queue> queue, GstBuffer *buffer,
                                          InferenceBackend::Image *image) {
    GstMemory *mem = gst_buffer_peek_memory(buffer, 0);
    if (!mem) {
        throw std::runtime_error("Failed to get GstBuffer memory");
    }
    int dma_fd;
    size_t dma_size = 1024;
    if (gst_is_dmabuf_memory(mem)) { // memory:DMABuf
        dma_fd = gst_dmabuf_memory_get_fd(mem);
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

        VADRMPRIMESurfaceDescriptor prime_desc{};
        vtable->vaExportSurfaceHandle(driver_context, va_surface_id, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                                      VA_EXPORT_SURFACE_READ_WRITE, &prime_desc);
        dma_fd = prime_desc.objects[0].fd;
        dma_size = prime_desc.objects[0].size;
        image->drm_format_modifier = prime_desc.objects[0].drm_format_modifier; // non-zero if tiled (non-linear) mem
        if (image) { // update stride and offset for each plane
            uint32_t n_planes = 0;
            for (uint32_t i = 0; i < prime_desc.num_layers; i++) {
                auto layer = &prime_desc.layers[i];
                for (uint32_t j = 0; j < layer->num_planes; j++) {
                    if (n_planes < InferenceBackend::Image::MAX_PLANES_NUMBER) {
                        image->stride[n_planes] = layer->pitch[j];
                        image->offsets[n_planes] = layer->offset[j];
                        n_planes++;
                    }
                }
            }
        }
    }

    ze_context_handle_t ze_context = queue->get_context().get_native<sycl::backend::level_zero>();
    ze_device_handle_t ze_device = queue->get_device().get_native<sycl::backend::level_zero>();
    void *ptr = nullptr;
    ze_result_t ze_res;

    ze_external_memory_import_fd_t import_fd = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD,
                                                nullptr, // pNext
                                                ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF, dma_fd};
    ze_device_mem_alloc_desc_t alloc_desc = {};
    alloc_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    alloc_desc.pNext = &import_fd;
    _dma_fd = dma_fd;
    ze_res = zeMemAllocDevice(ze_context, &alloc_desc, dma_size, 1, ze_device, &ptr);

    if (ze_res != ZE_RESULT_SUCCESS) {
        throw std::runtime_error("Failed to get USM pointer");
    }
    if (!gst_is_dmabuf_memory(mem)) { // memory:VASurface
        close(dma_fd);
    }

    return ptr;
}
