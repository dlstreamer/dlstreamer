/*******************************************************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "vaapi_utils.h"

#include "inference_backend/image.h"

#include <functional>
#include <stdexcept>

#include <va/va.h>

namespace InferenceBackend {

class VaApiContext {
  private:
    VADisplay _va_display = nullptr;
    VAConfigID _va_config = VA_INVALID_ID;
    VAContextID _va_context_id = VA_INVALID_ID;
    int _dri_file_descriptor = 0;
    bool _own_va_display = false;
    std::function<void(const char *)> message_callback;

  public:
    explicit VaApiContext(VADisplay va_display);
    VaApiContext();

    ~VaApiContext();

    VADisplay Display();
    VAContextID Id();
};

} // namespace InferenceBackend
