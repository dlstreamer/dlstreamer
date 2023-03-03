/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/tensor.h"

namespace dlstreamer {

namespace tensor::key {
static constexpr auto va_surface_ptr = "va_surface_ptr"; // VASurfaceId*
};

class VAAPITensor : public BaseTensor {
  public:
    using VASurfaceID = uint32_t;

    VAAPITensor(VASurfaceID va_surface, int plane_index, const TensorInfo &info, ContextPtr context)
        : BaseTensor(MemoryType::VAAPI, info, tensor::key::va_surface_ptr, context), _va_surface(va_surface) {
        set_handle(tensor::key::va_surface_ptr, reinterpret_cast<handle_t>(&_va_surface));
        set_handle(tensor::key::plane_index, plane_index);
    }

    VASurfaceID va_surface() {
        return _va_surface;
    }

    int plane_index() {
        return handle(tensor::key::plane_index);
    }

    int offset_x() {
        return handle(tensor::key::offset_x, 0);
    }

    int offset_y() {
        return handle(tensor::key::offset_y, 0);
    }

  protected:
    VASurfaceID _va_surface;
};

using VAAPITensorPtr = std::shared_ptr<VAAPITensor>;

} // namespace dlstreamer
