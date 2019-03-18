/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/pre_proc.h"
#include "va_image_locker.h"
#include <va/va.h>

namespace InferenceBackend {

class VAAPI_VPP : public PreProc {
  public:
    ~VAAPI_VPP();

    void Convert(const Image &src, Image &dst, bool bAllocateDestination);

    void ReleaseImage(const Image &dst);

  protected:
    int dst_width = 0;
    int dst_height = 0;
    VAImageFormat dst_format = {};
    VADisplay va_display = 0;
    VAConfigID va_config = 0;
    VAContextID va_context = 0;
    VASurfaceID va_surface = 0;
    VAImageLocker locker;

    VAStatus Init(VADisplay va_display, int dst_width, int dst_height, int format_fourcc);

    void Close();

    int Fourcc2RTFormat(int format_fourcc) const;
};

} // namespace InferenceBackend
