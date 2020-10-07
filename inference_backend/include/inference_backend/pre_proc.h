/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "image.h"

namespace InferenceBackend {

enum class ImagePreprocessorType { INVALID, OPENCV, IE, VAAPI_SYSTEM, VAAPI_SURFACE_SHARING };
class ImagePreprocessor {
  public:
    static ImagePreprocessor *Create(ImagePreprocessorType type);

    virtual ~ImagePreprocessor() = default;

    virtual void Convert(const Image &src, Image &dst, bool bAllocateDestination = false) = 0;
    virtual void ReleaseImage(const Image &dst) = 0; // to be called if Convert called with bAllocateDestination = true
};

int GetPlanesCount(int fourcc);
Image ApplyCrop(const Image &src);

ImagePreprocessor *CreatePreProcOpenCV();
} // namespace InferenceBackend
