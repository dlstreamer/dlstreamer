/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "image.h"
#include "input_image_layer_descriptor.h"

namespace InferenceBackend {

enum class ImagePreprocessorType : int { AUTO = 0, OPENCV, IE, VAAPI_SYSTEM, VAAPI_SURFACE_SHARING };
class ImagePreprocessor {
  public:
    static ImagePreprocessor *Create(ImagePreprocessorType type);

    virtual ~ImagePreprocessor() = default;

    virtual void Convert(const Image &src, Image &dst, const InputImageLayerDesc::Ptr &pre_proc_info = nullptr,
                         const ImageTransformationParams::Ptr &image_transform_info = nullptr,
                         bool bAllocateDestination = false) = 0;
    virtual void ReleaseImage(const Image &dst) = 0; // to be called if Convert called with bAllocateDestination = true

  protected:
    bool doNeedPreProcessing(const Image &src, Image &dst);
    bool doNeedCustomImageConvert(const InputImageLayerDesc::Ptr &pre_proc_info);
};

int GetPlanesCount(int fourcc);
Image ApplyCrop(const Image &src);

ImagePreprocessor *CreatePreProcOpenCV();
} // namespace InferenceBackend
