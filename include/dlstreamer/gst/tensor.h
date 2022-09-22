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

class GSTTensor : public BaseTensor {
  public:
    struct key {
        static constexpr auto gst_memory = "gst_memory"; // (GstMemory*)
        static constexpr auto offset_x = "offset_x";     // int
        static constexpr auto offset_y = "offset_y";     // int
    };

    GSTTensor(const TensorInfo &info, GstMemory *mem, bool take_ownership, ContextPtr context)
        : BaseTensor(MemoryType::GST, info, key::gst_memory, context), _take_ownership(take_ownership) {
        set_handle(key::gst_memory, reinterpret_cast<handle_t>(mem));
    }
    ~GSTTensor() {
        GstMemory *mem = gst_memory();
        if (mem && _take_ownership) {
            gst_memory_unref(mem);
        }
    }
    GstMemory *gst_memory() const {
        return reinterpret_cast<GstMemory *>(handle(key::gst_memory));
    }
    operator GstMemory *() const {
        return gst_memory();
    }
    int offset_x() const {
        return handle(key::offset_x, 0);
    }
    int offset_y() const {
        return handle(key::offset_y, 0);
    }

    void crop(int x, int y, int w, int h) {
        ImageInfo image_info(_info);
        set_handle(key::offset_x, x);
        set_handle(key::offset_y, y);
        _info.shape[image_info.layout().w_position()] = w;
        _info.shape[image_info.layout().h_position()] = h;
    }

  protected:
    bool _take_ownership;
};

using GstTensorPtr = std::shared_ptr<GSTTensor>;

} // namespace dlstreamer
