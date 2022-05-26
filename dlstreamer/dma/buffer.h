/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_base.h"

#ifdef __linux__
#include <unistd.h>
#endif

namespace dlstreamer {

class DMABuffer : public BufferBase {
  public:
    static constexpr auto dma_fd_id = "dma_fd";             // (int)
    static constexpr auto drm_modifier_id = "drm_modifier"; // (int)

    DMABuffer(int dma_fd, int drm_modifier, BufferInfoCPtr info, bool take_ownership = false)
        : BufferBase(BufferType::DMA_FD, info), _take_ownership(take_ownership) {
#ifdef __linux__
        set_handle(dma_fd_id, 0, dma_fd);
        set_handle(drm_modifier_id, 0, drm_modifier);
#else
        throw std::runtime_error("DMABuffer is not supported");
#endif
    }

    int fd() const {
        return handle(dma_fd_id);
    }

    ~DMABuffer() {
#ifdef __linux__
        if (_take_ownership) {
            close(handle(dma_fd_id));
        }
#endif
    }

  private:
    bool _take_ownership = false;
};

using DMABufferPtr = std::shared_ptr<DMABuffer>;

} // namespace dlstreamer
