/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dma_to_usm.h"

#include <level_zero/ze_api.h>
#include <unistd.h>

#include <dlstreamer/dma/tensor.h>
#include <dlstreamer/level_zero/context.h>
#include <dlstreamer/level_zero/usm_tensor.h>

namespace dlstreamer {

MapperDMAToUSM::MapperDMAToUSM(MemoryMapperPtr input_buffer_mapper, ContextPtr usm_context)
    : BaseMemoryMapper(nullptr, usm_context), _input_mapper(std::move(input_buffer_mapper)),
      _context(std::dynamic_pointer_cast<LevelZeroContext>(usm_context)) {
    if (!_context)
        throw std::invalid_argument("Invalid context type: USM context is expected");
    if (!_input_mapper)
        throw std::invalid_argument("input_buffer_mapper is null");
};

TensorPtr MapperDMAToUSM::map(TensorPtr buffer, AccessMode mode) {
    auto dma_buf = _input_mapper->map(buffer, mode);
    if (dma_buf->memory_type() != MemoryType::DMA)
        throw std::runtime_error("DMA buffer is expected as mapped memory type");

    size_t dma_size = dma_buf->info().nbytes();
    int dma_fd = dma_buf->handle(DMATensor::key::dma_fd);

    void *usm_ptr = getDeviceMemPointer(dma_fd, dma_size);

    auto usm_tensor = new USMTensor(dma_buf->info(), usm_ptr, false, nullptr);

    // FIXME: move deleter to USMBuffer ?
    auto deleter = [this, usm_ptr](USMTensor *usm_tensor) mutable {
        zeMemFree(_context->ze_context(), usm_ptr);
        delete usm_tensor;
    };

    return std::shared_ptr<USMTensor>(usm_tensor, deleter);
}

void *MapperDMAToUSM::getDeviceMemPointer(int dma_fd, size_t dma_size) {
    ze_context_handle_t ze_context = _context->ze_context();
    ze_device_handle_t ze_device = _context->ze_device();

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
