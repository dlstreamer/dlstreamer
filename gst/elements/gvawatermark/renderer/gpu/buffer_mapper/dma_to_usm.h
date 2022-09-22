/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/base/memory_mapper.h>

namespace dlstreamer {

class MapperDMAToUSM : public BaseMemoryMapper {
  public:
    MapperDMAToUSM(MemoryMapperPtr input_buffer_mapper, ContextPtr usm_context);

    using BaseMemoryMapper::map;

    TensorPtr map(TensorPtr src_buffer, AccessMode mode) override;

  protected:
    MemoryMapperPtr _input_mapper;
    std::shared_ptr<class LevelZeroContext> _context;

    void *getDeviceMemPointer(int dma_fd, size_t dma_size);
};

} // namespace dlstreamer
