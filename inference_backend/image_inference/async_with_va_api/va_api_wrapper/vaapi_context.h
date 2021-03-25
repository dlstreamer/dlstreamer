/*******************************************************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "vaapi_utils.h"

#include "inference_backend/image.h"

#include <functional>
#include <set>
#include <stdexcept>

namespace InferenceBackend {

class VaApiContext {
  public:
    explicit VaApiContext(VADisplay va_display);
    VaApiContext();

    ~VaApiContext();

    /* getters */
    VADisplay DisplayRaw() const;
    VaDpyWrapper Display() const;
    VAContextID Id() const;
    int RTFormat() const;
    bool IsPixelFormatSupported(int format) const;

  private:
    VaDpyWrapper _display;
    VAConfigID _va_config_id = VA_INVALID_ID;
    VAContextID _va_context_id = VA_INVALID_ID;
    int _dri_file_descriptor = 0;
    int _rt_format = VA_RT_FORMAT_YUV420;
    std::set<int> _supported_pixel_formats;
    bool _own_va_display = false;

    /* private helper methods */
    void create_va_display_and_device_descriptor();
    void create_config_and_contexts();
    void create_supported_pixel_formats();
    void set_callbacks_and_initialize_va_display();
};

} // namespace InferenceBackend
