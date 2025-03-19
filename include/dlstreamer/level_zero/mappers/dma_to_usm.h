/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/memory_mapper.h"
#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/level_zero/usm_tensor.h"
#include "dlstreamer/utils.h"

#include <level_zero/ze_api.h>

namespace dlstreamer {

class MemoryMapperDMAToUSM : public BaseMemoryMapper {
  public:
    MemoryMapperDMAToUSM(const ContextPtr &input_context, const ContextPtr &output_context)
        : BaseMemoryMapper(input_context, output_context) {
        DLS_CHECK(_ze_context = ze_context_handle_t(output_context->handle(BaseContext::key::ze_context)));
        DLS_CHECK(_ze_device = ze_device_handle_t(output_context->handle(BaseContext::key::ze_device)));
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        int32_t dma_fd = ptr_cast<DMATensor>(src)->dma_fd();
        size_t dma_size = src->info().nbytes();

        // WA for issue in Level Zero when zeMemFree called FD that was passed to export external memory will be
        // closed but it shouldn`t.
        // This can be removed once fixed in Level Zero.
        // dma_fd = dup(dma_fd);

        ze_external_memory_import_fd_t import_fd = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD,
                                                    nullptr, // pNext
                                                    ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF, dma_fd};
        ze_device_mem_alloc_desc_t alloc_desc = {};
        alloc_desc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        alloc_desc.pNext = &import_fd;

        void *ptr = nullptr;
        ze_result_t ze_res = zeMemAllocDevice(_ze_context, &alloc_desc, dma_size, 1, _ze_device, &ptr);
        if (ze_res != ZE_RESULT_SUCCESS)
            throw std::runtime_error("Failed to convert DMA-BUF to USM pointer: " + std::to_string(ze_res));

        auto dst = std::make_shared<USMTensor>(src->info(), ptr, true, _output_context);
        dst->set_parent(src);
        return dst;
    }

  private:
    ze_device_handle_t _ze_device;
    ze_context_handle_t _ze_context;
};

} // namespace dlstreamer
