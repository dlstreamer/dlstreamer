/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/pre_proc.h"
#include <va/va.h>

namespace InferenceBackend {

class VAAPIPreProc : public PreProc {
  public:
    VAAPIPreProc();
    ~VAAPIPreProc();

    void Convert(const Image &src, Image &dst, bool bAllocateDestination);

    void ReleaseImage(const Image &dst);

  protected:
    VADisplay va_display = nullptr;
    VAConfigID va_config = VA_INVALID_ID;
    VAContextID va_context = VA_INVALID_ID;

    VAStatus Init(VADisplay va_display, int dst_width, int dst_height, int format_fourcc);
};

} // namespace InferenceBackend
