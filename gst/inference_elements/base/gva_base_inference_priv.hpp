/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef __cplusplus

#include <memory>

// FWD
class BufferMapper;

// Channel (GvaBaseInference) specific information. Contains C++ objects
struct GvaBaseInferencePrivate {
    // Decoder VA display, if present
    std::shared_ptr<void> va_display;

    std::unique_ptr<BufferMapper> buffer_mapper;
};

#endif // __cplusplus
