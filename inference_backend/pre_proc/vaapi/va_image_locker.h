/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <va/va.h>

class VAImageLocker {
  public:
    VAImageLocker();
    ~VAImageLocker();

    VAStatus Map(VADisplay va_display, VASurfaceID surface_id, VAImageFormat *va_format = NULL, int width = 0,
                 int height = 0);
    void GetImageBuffer(uint8_t **planes, int *stride);
    VAStatus Unmap();

  protected:
    VADisplay va_display;
    VAImage va_image;
    void *surface_p;
};
