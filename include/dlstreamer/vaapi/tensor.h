/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/tensor.h"

namespace dlstreamer {

class VAAPITensor : public BaseTensor {
  public:
    struct key {
        static constexpr auto va_surface_ptr = "va_surface_ptr"; // VASurfaceId*
        static constexpr auto plane_index = "plane_index";       // int
        static constexpr auto offset_x = "offset_x";             // int
        static constexpr auto offset_y = "offset_y";             // int
    };
    using VASurfaceID = uint32_t;

    VAAPITensor(VASurfaceID va_surface, int plane_index, const TensorInfo &info, ContextPtr context)
        : BaseTensor(MemoryType::VAAPI, info, key::va_surface_ptr, context), _va_surface(va_surface) {
        set_handle(key::va_surface_ptr, reinterpret_cast<handle_t>(&_va_surface));
        set_handle(key::plane_index, plane_index);
    }

    VASurfaceID va_surface() {
        return _va_surface;
    }

    int plane_index() {
        return handle(key::plane_index);
    }

    int offset_x() {
        return handle(key::offset_x, 0);
    }

    int offset_y() {
        return handle(key::offset_y, 0);
    }

  protected:
    VASurfaceID _va_surface;
};

using VAAPITensorPtr = std::shared_ptr<VAAPITensor>;

} // namespace dlstreamer
