/*******************************************************************************
 * Copyright (C) <2018-2019> Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/pre_proc.h"
#include "config.h"

namespace InferenceBackend {

PreProc *CreatePreProcOpenCV();
PreProc *CreatePreProcVAAPI();

PreProc *PreProc::Create(MemoryType type) {
#ifndef DISABLE_VAAPI
    if (type == MemoryType::VAAPI)
        return CreatePreProcVAAPI();
#endif
    if (type == MemoryType::SYSTEM)
        return CreatePreProcOpenCV();
    return nullptr;
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
} // namespace InferenceBackend
