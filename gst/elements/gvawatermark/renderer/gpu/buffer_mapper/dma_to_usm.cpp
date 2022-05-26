/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dma_to_usm.h"

#include <level_zero/ze_api.h>
#include <unistd.h>

#include <dlstreamer/dma/buffer.h>
#include <dlstreamer/usm/buffer.h>
#include <dlstreamer/usm/context.h>

namespace dlstreamer {

BufferMapperDmaToUsm::BufferMapperDmaToUsm(BufferMapperPtr input_buffer_mapper, ContextPtr usm_context)
    : _input_mapper(std::move(input_buffer_mapper)), _context(std::dynamic_pointer_cast<UsmContext>(usm_context)) {
    if (!_context)
        throw std::invalid_argument("Invalid context type: USM context is expected");
    if (!_input_mapper)
        throw std::invalid_argument("input_buffer_mapper is null");
};

BufferPtr BufferMapperDmaToUsm::map(BufferPtr buffer, AccessMode mode) {
    auto dma_buf = _input_mapper->map(buffer, mode);
    if (dma_buf->type() != BufferType::DMA_FD)
        throw std::runtime_error("DMA buffer is expected as mapped memory type");

    size_t dma_size = dma_buf->info()->planes.front().size();
    int dma_fd = dma_buf->handle(DMABuffer::dma_fd_id);

    void *usm_ptr = getDeviceMemPointer(dma_fd, dma_size);

    auto usm_buffer = new UsmBuffer(dma_buf->info(), usm_ptr);

    // FIXME: move deleter to UsmBuffer ?
    auto deleter = [this, usm_ptr, dma_buf](UsmBuffer *usm_buffer) mutable {
        zeMemFree(_context->context_handle(), usm_ptr);
        // Release reference to source DMA buffer
        dma_buf.reset();
        delete usm_buffer;
    };

    return std::shared_ptr<UsmBuffer>(usm_buffer, deleter);
}

void *BufferMapperDmaToUsm::getDeviceMemPointer(int dma_fd, size_t dma_size) {
    ze_context_handle_t ze_context = _context->context_handle();
    ze_device_handle_t ze_device = _context->device_handle();

    // WA for issue in Level Zero when zeMemFree called FD that was passed to export external memory will be
    // closed but it shouldn`t.
    // This can be removed once fixed in Level Zero.
    dma_fd = dup(dma_fd);

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

} // namespace dlstreamer
