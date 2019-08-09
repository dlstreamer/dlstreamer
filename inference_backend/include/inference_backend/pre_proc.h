/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "image.h"

namespace InferenceBackend {

enum class PreProcessType {
    Invalid,
    OpenCV,
    GAPI,
    VAAPI,
};

class PreProc {
  public:
    static PreProc *Create(PreProcessType type);

    virtual ~PreProc() {
    }

    virtual void Convert(const Image &src, Image &dst, bool bAllocateDestination = false) = 0;
    virtual void ReleaseImage(const Image &dst) = 0; // to be called if Convert called with bAllocateDestination = true
};

int GetPlanesCount(int fourcc);
Image ApplyCrop(const Image &src);

PreProc *CreatePreProcGAPI();
PreProc *CreatePreProcOpenCV();
PreProc *CreatePreProcVAAPI();
} // namespace InferenceBackend
