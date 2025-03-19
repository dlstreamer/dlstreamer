/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/dma/tensor.h"
#include "dlstreamer/gst/frame.h"
#include <gst/allocators/allocators.h>

namespace dlstreamer {

class MemoryMapperGSTToDMA : public BaseMemoryMapper {
  public:
    using BaseMemoryMapper::BaseMemoryMapper;

    TensorPtr map(TensorPtr src, AccessMode /*mode*/) override {
        auto gst_tensor = ptr_cast<GSTTensor>(src);
        int dma_fd = gst_dmabuf_memory_get_fd(gst_tensor->gst_memory());
        if (dma_fd < 0)
            throw std::runtime_error("Failed to import DMA buffer FD");

        auto dst = std::make_shared<DMATensor>(dma_fd, 0, src->info());

        // offset and offset_x/y
        int data_offset = src->handle(tensor::key::offset, 0);
        int offset_x = gst_tensor->offset_x();
        int offset_y = gst_tensor->offset_y();
        if (offset_x || offset_y) {
            ImageInfo image_info(src->info());
            data_offset += offset_y * image_info.width_stride() + offset_x * image_info.channels_stride();
        }

        dst->set_handle(tensor::key::offset, data_offset);
        dst->set_parent(src);
        return dst;
    }
};

} // namespace dlstreamer
