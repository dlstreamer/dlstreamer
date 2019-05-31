/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/pre_proc.h"
#include "config.h"

using namespace InferenceBackend;

namespace InferenceBackend {

PreProc *PreProc::Create(MemoryType type) {
    if (type == MemoryType::SYSTEM) {
#ifdef HAVE_GAPI
        return CreatePreProcGAPI();
#endif
        return CreatePreProcOpenCV();
    }

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
