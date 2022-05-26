/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef __cplusplus

#include "inference_backend/buffer_mapper.h"

#include <memory>

// FWD
class BufferMapper;

// Channel (GvaBaseInference) specific information. Contains C++ objects
struct GvaBaseInferencePrivate {
    // Decoder VA display, if present
    dlstreamer::ContextPtr va_display;

    std::unique_ptr<InferenceBackend::BufferMapper> buffer_mapper;
};

#endif // __cplusplus
