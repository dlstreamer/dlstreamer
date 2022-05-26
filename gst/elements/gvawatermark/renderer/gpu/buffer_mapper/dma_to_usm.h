/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/buffer_mapper.h>

namespace dlstreamer {

class BufferMapperDmaToUsm : public BufferMapper {
    BufferMapperPtr _input_mapper;
    BufferMapperPtr _vaapi_to_dma_mapper;
    std::shared_ptr<class UsmContext> _context;

    void *getDeviceMemPointer(int dma_fd, size_t dma_size);

  public:
    BufferMapperDmaToUsm(BufferMapperPtr input_buffer_mapper, ContextPtr usm_context);

    BufferPtr map(BufferPtr src_buffer, AccessMode mode) override;
};

} // namespace dlstreamer
