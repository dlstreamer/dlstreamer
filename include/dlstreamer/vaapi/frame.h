/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/base/frame.h"
#include "dlstreamer/frame_info.h"
#include "dlstreamer/vaapi/tensor.h"

namespace dlstreamer {

class VAAPIFrame : public BaseFrame {
  public:
    VAAPIFrame(VAAPITensor::VASurfaceID va_surface, const FrameInfo &info, ContextPtr context)
        : BaseFrame(MediaType::Image, 0, MemoryType::VAAPI) {
        for (size_t i = 0; i < info.tensors.size(); i++) {
            _tensors.push_back(std::make_shared<VAAPITensor>(va_surface, i, info.tensors[i], context));
        }
    }

    VAAPITensor::VASurfaceID va_surface(int plane_index = 0) {
        // return *(VAAPITensor::VASurfaceID*)_tensors[plane_index]->handle(VAAPITensor::key::va_surface_ptr);
        return ptr_cast<VAAPITensor>(_tensors[plane_index])->va_surface();
    }
};

using VAAPIFramePtr = std::shared_ptr<VAAPIFrame>;

} // namespace dlstreamer
