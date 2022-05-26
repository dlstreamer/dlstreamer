/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/buffer_base.h"

namespace dlstreamer {

class VAAPIBuffer : public BufferBase {
  public:
    static constexpr auto va_surface_id = "vaapi.surface"; // VASurfaceId
    using VASurfaceID = unsigned int;

    VAAPIBuffer(VASurfaceID va_surface, BufferInfoCPtr info, ContextPtr context)
        : BufferBase(BufferType::VAAPI_SURFACE, info, context) {
        set_handle(va_surface_id, 0, va_surface);
    }

    VASurfaceID va_surface() {
        return handle(va_surface_id, 0);
    }
};

using VAAPIBufferPtr = std::shared_ptr<VAAPIBuffer>;

} // namespace dlstreamer
