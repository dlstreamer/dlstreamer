/*******************************************************************************
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"

#ifdef __linux__
#include <unistd.h>
#endif

namespace dlstreamer {

namespace tensor::key {
static constexpr auto dma_fd = "dma_fd";             // (int)
static constexpr auto drm_modifier = "drm_modifier"; // (int)
};                                                   // namespace tensor::key

class DMATensor : public BaseTensor {
  public:
    DMATensor(int dma_fd, int drm_modifier, const TensorInfo &info, bool take_ownership = false,
              ContextPtr context = nullptr)
        : BaseTensor(MemoryType::DMA, info, tensor::key::dma_fd, context), _take_ownership(take_ownership) {
#ifndef __linux__
        throw std::runtime_error("DMABuffer is not supported");
#endif
        set_handle(tensor::key::dma_fd, dma_fd);
        set_handle(tensor::key::drm_modifier, drm_modifier);
    }

    int dma_fd() const {
        return handle(tensor::key::dma_fd);
    }

    int drm_modifier() const {
        return handle(tensor::key::drm_modifier);
    }

    size_t offset() const {
        return handle(tensor::key::offset, 0);
    }

    ~DMATensor() {
#ifdef __linux__
        if (_take_ownership) {
            close(handle(tensor::key::dma_fd));
        }
#endif
    }

  private:
    bool _take_ownership = false;
};

using DMATensorPtr = std::shared_ptr<DMATensor>;

} // namespace dlstreamer
