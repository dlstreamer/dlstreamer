/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"
#include <va/va.h>

namespace InferenceBackend {

class VaApiImageMap : public ImageMap {
  public:
    VaApiImageMap();
    ~VaApiImageMap();

    Image Map(const Image &image) override;
    void Unmap() override;

  protected:
    VADisplay va_display;
    VAImage va_image;
};

} // namespace InferenceBackend
