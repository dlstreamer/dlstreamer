/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"
#include <va/va.h>

namespace InferenceBackend {

class VAAPIImageMap : public ImageMap {
  public:
    VAAPIImageMap();
    ~VAAPIImageMap();

    Image Map(const Image &image);
    void Unmap();

  protected:
    VADisplay va_display;
    VAImage va_image;
};

} // namespace InferenceBackend
