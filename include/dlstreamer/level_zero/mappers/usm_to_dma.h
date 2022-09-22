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

class MemoryMapperUSMToDMA : public BaseMemoryMapper {
  public:
    MemoryMapperUSMToDMA(const ContextPtr &input_context, const ContextPtr &output_context)
        : BaseMemoryMapper(input_context, output_context) {
        DLS_CHECK(_ze_context = ze_context_handle_t(output_context->handle(BaseContext::key::ze_context)));
    }

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        ze_external_memory_export_fd_t export_fd = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_FD, nullptr,
                                                    ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF, 0};
        ze_memory_allocation_properties_t alloc_props = {};
        alloc_props.stype = ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES;
        alloc_props.pNext = &export_fd;
        ze_result_t ze_res = zeMemGetAllocProperties(_ze_context, src->data(), &alloc_props, nullptr);
        if (ze_res != ZE_RESULT_SUCCESS)
            throw std::runtime_error("Failed to convert USM pointer to DMA-BUF: " + std::to_string(ze_res));

        auto dma_fd = export_fd.fd;
        auto dst = std::make_shared<DMATensor>(dma_fd, 0, src->info(), true, _output_context);
        dst->set_parent(src);
        return dst;
    }

  private:
    ze_context_handle_t _ze_context;
};

} // namespace dlstreamer
