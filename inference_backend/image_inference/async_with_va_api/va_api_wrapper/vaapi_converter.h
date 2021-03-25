/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "vaapi_context.h"
#include "vaapi_images.h"
#include "vaapi_utils.h"

#include <memory>

namespace InferenceBackend {

class VaApiConverter {
    VaApiContext *_context;

  public:
    explicit VaApiConverter(VaApiContext *context);
    ~VaApiConverter() = default;

    void Convert(const Image &src, InferenceBackend::VaApiImage &va_api_dst);
};

} // namespace InferenceBackend
