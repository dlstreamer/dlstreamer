/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/pre_proc.h"
#include "config.h"
#include <functional>

namespace InferenceBackend {

PreProc *PreProc::Create(PreProcessType type) {
    PreProc *pProc = nullptr;
    switch (type) {
    case PreProcessType::OpenCV:
        pProc = CreatePreProcOpenCV();
        break;
    case PreProcessType::GAPI:
        pProc = CreatePreProcGAPI();
        break;
#ifdef HAVE_VAAPI
    case PreProcessType::VAAPI:
        pProc = CreatePreProcVAAPI();
        break;
#endif
    }
    if (pProc == nullptr)
        throw std::runtime_error("ERROR: Failed to create a preprocessor\n");
    return pProc;
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

    if (src.rect.x >= src.width || src.rect.y >= src.height || src.rect.x + src.rect.width <= 0 ||
        src.rect.y + src.rect.height <= 0) {
        throw std::runtime_error("ERROR: ApplyCrop: Requested rectangle is out of image boundaries\n");
    }

    int rect_x = std::max(src.rect.x, 0);
    int rect_y = std::max(src.rect.y, 0);
    int rect_width = std::min(src.rect.width - (rect_x - src.rect.x), src.width - rect_x);
    int rect_height = std::min(src.rect.height - (rect_y - src.rect.y), src.height - rect_y);

    switch (src.format) {
    case InferenceBackend::FOURCC_NV12: {
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x;
        dst.planes[1] = src.planes[1] + (rect_y / 2) * src.stride[1] + rect_x;
        break;
    }
    case InferenceBackend::FOURCC_I420: {
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x;
        dst.planes[1] = src.planes[1] + (rect_y / 2) * src.stride[1] + (rect_x / 2);
        dst.planes[2] = src.planes[2] + (rect_y / 2) * src.stride[2] + (rect_x / 2);
        break;
    }
    case InferenceBackend::FOURCC_RGBP: {
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x;
        dst.planes[1] = src.planes[1] + rect_y * src.stride[1] + rect_x;
        dst.planes[2] = src.planes[2] + rect_y * src.stride[2] + rect_x;
        break;
    }
    case InferenceBackend::FOURCC_BGR: {
        int channels = 3;
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x * channels;
        break;
    }
    default: {
        int channels = 4;
        dst.planes[0] = src.planes[0] + rect_y * src.stride[0] + rect_x * channels;
        break;
    }
    }

    if (rect_width)
        dst.width = rect_width;
    if (rect_height)
        dst.height = rect_height;

    return dst;
}
} // namespace InferenceBackend
