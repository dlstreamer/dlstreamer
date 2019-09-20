/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "vaapi_utils.h"

#include "inference_backend/image.h"

#include <stdexcept>

#include <va/va.h>

namespace InferenceBackend {

class VaApiContext {
  private:
    MemoryType _memory_type;
    VADisplay _va_display = nullptr;
    VAConfigID _va_config = VA_INVALID_ID;
    VAContextID _va_context_id = VA_INVALID_ID;
    int _dri_file_descriptor = 0;
    bool _own_va_display = false;

  public:
    explicit VaApiContext(MemoryType memory_type, VADisplay va_display = nullptr);
    ~VaApiContext();

    MemoryType GetMemoryType();
    VADisplay Display();
    VAContextID Id();
};

} // namespace InferenceBackend
