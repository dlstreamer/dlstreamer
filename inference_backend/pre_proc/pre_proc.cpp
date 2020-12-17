/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/safe_arithmetic.h"
#include <functional>

#include "config.h"
#include "inference_backend/pre_proc.h"

namespace InferenceBackend {

ImagePreprocessor *ImagePreprocessor::Create(ImagePreprocessorType type) {
    ImagePreprocessor *p = nullptr;
    switch (type) {
    case ImagePreprocessorType::OPENCV:
        p = CreatePreProcOpenCV();
        break;
#ifdef ENABLE_VAAPI
    case ImagePreprocessorType::VAAPI_SYSTEM:
        p = CreatePreProcOpenCV();
        break;
#endif
    }
    if (p == nullptr)
        throw std::runtime_error("Failed to allocate Image preprocessor");
    return p;
}

int GetPlanesCount(int fourcc) {
    switch (fourcc) {
    case FOURCC_BGRA:
    case FOURCC_BGRX:
    case FOURCC_BGR:
    case FOURCC_RGBA:
    case FOURCC_RGBX:
        return 1;
    case FOURCC_NV12:
        return 2;
    case FOURCC_BGRP:
    case FOURCC_RGBP:
    case FOURCC_I420:
        return 3;
    }

    return 0;
}

Image ApplyCrop(const Image &src) {
    // GVA_DEBUG(__FUNCTION__);
    Image dst = src;
    int planes_count = GetPlanesCount(src.format);
    if (!src.rect.width && !src.rect.height) {
        for (int i = 0; i < planes_count; i++)
            dst.planes[i] = src.planes[i];
        return dst;
    }

    dst.rect = {};

    if (src.width <= src.rect.x or src.height <= src.rect.y)
        throw std::logic_error("ApplyCrop: Requested rectangle is out of image boundaries.");

    dst.width = std::min(src.rect.width, src.width - src.rect.x);
    dst.height = std::min(src.rect.height, src.height - src.rect.y);

    switch (src.format) {
    case InferenceBackend::FOURCC_NV12: {
        dst.planes[0] = src.planes[0] + src.rect.y * src.stride[0] + src.rect.x;
        dst.planes[1] = src.planes[1] + (src.rect.y / 2) * src.stride[1] + src.rect.x;
        break;
    }
    case InferenceBackend::FOURCC_I420: {
        dst.planes[0] = src.planes[0] + src.rect.y * src.stride[0] + src.rect.x;
        dst.planes[1] = src.planes[1] + (src.rect.y / 2) * src.stride[1] + (src.rect.x / 2);
        dst.planes[2] = src.planes[2] + (src.rect.y / 2) * src.stride[2] + (src.rect.x / 2);
        break;
    }
    case InferenceBackend::FOURCC_RGBP: {
        dst.planes[0] = src.planes[0] + src.rect.y * src.stride[0] + src.rect.x;
        dst.planes[1] = src.planes[1] + src.rect.y * src.stride[1] + src.rect.x;
        dst.planes[2] = src.planes[2] + src.rect.y * src.stride[2] + src.rect.x;
        break;
    }
    case InferenceBackend::FOURCC_BGR: {
        const uint32_t channels = 3;
        dst.planes[0] = src.planes[0] + src.rect.y * src.stride[0] + src.rect.x * channels;
        break;
    }
    case InferenceBackend::FOURCC_BGRX:
    case InferenceBackend::FOURCC_BGRA: {
        const uint32_t channels = 4;
        dst.planes[0] = src.planes[0] + src.rect.y * src.stride[0] + src.rect.x * channels;
        break;
    }
    default: {
        throw std::invalid_argument("Unsupported image format for crop");
        break;
    }
    }

    return dst;
}

bool ImagePreprocessor::doNeedCustomImageConvert(const InputImageLayerDesc::Ptr &pre_proc_info) {
    if (pre_proc_info)
        if (pre_proc_info->isDefined())
            return true;
    return false;
}

bool ImagePreprocessor::doNeedPreProcessing(const Image &src, Image &dst) {
    if (src.format == dst.format and (src.format == FourCC::FOURCC_RGBP or src.format == FourCC::FOURCC_RGBP_F32) and
        src.width == dst.width and src.height == dst.height) {
        return true;
    }
    return false;
}

} // namespace InferenceBackend
