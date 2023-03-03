/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/tensor.h"
#include "dlstreamer/gst/utils.h"
#include <stdexcept>

namespace dlstreamer {

namespace tensor::key {
static constexpr auto gst_memory = "gst_memory"; // (GstMemory*)
}

class GSTTensor : public BaseTensor {
  public:
    GSTTensor(const TensorInfo &info, GstMemory *mem, bool take_ownership, ContextPtr context, int planeIdx = 0)
        : BaseTensor(MemoryType::GST, info, tensor::key::gst_memory, context), _take_ownership(take_ownership) {
        set_handle(tensor::key::gst_memory, reinterpret_cast<handle_t>(mem));
        set_handle(tensor::key::plane_index, planeIdx);
    }
    ~GSTTensor() {
        GstMemory *mem = gst_memory();
        if (mem && _take_ownership) {
            gst_memory_unref(mem);
        }
    }
    GstMemory *gst_memory() const {
        return reinterpret_cast<GstMemory *>(handle(tensor::key::gst_memory));
    }
    operator GstMemory *() const {
        return gst_memory();
    }
    int offset_x() const {
        return handle(tensor::key::offset_x, 0);
    }
    int offset_y() const {
        return handle(tensor::key::offset_y, 0);
    }
    int plane_index() const {
        return handle(tensor::key::plane_index);
    }

    void crop(int x, int y, int w, int h) {
        ImageInfo image_info(_info);
        set_handle(tensor::key::offset_x, x);
        set_handle(tensor::key::offset_y, y);
        _info.shape[image_info.layout().w_position()] = w;
        _info.shape[image_info.layout().h_position()] = h;
    }

  protected:
    bool _take_ownership;
};

using GstTensorPtr = std::shared_ptr<GSTTensor>;

} // namespace dlstreamer
