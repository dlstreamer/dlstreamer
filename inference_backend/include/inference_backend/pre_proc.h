/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "image.h"

namespace InferenceBackend {

class PreProc {
  public:
    static PreProc *Create(MemoryType type);

    virtual ~PreProc(){};

    virtual void Convert(const Image &src, Image &dst, bool bAllocateDestination = false) = 0;

    virtual void ReleaseImage(const Image &dst) = 0; // to be called if Convert called with bAllocateDestination = true
};

int GetPlanesCount(int fourcc);

} // namespace InferenceBackend
