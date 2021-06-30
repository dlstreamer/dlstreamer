/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gstcaps.h>

#include <inference_backend/image.h>

#define DMABUF_FEATURE_STR "memory:DMABuf"
#define VASURFACE_FEATURE_STR "memory:VASurface"

InferenceBackend::MemoryType get_memory_type_from_caps(const GstCaps *caps);
