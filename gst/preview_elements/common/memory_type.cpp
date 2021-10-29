/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "memory_type.hpp"

using namespace InferenceBackend;

MemoryType get_memory_type_from_caps(const GstCaps *caps) {
    if (caps == nullptr)
        return MemoryType::SYSTEM;

    for (auto i = 0u; i < gst_caps_get_size(caps); i++) {
        auto features = gst_caps_get_features(caps, i);
        if (gst_caps_features_contains(features, DMABUF_FEATURE_STR))
            return MemoryType::DMA_BUFFER;
        if (gst_caps_features_contains(features, VASURFACE_FEATURE_STR))
            return MemoryType::VAAPI;
    }

    return MemoryType::SYSTEM;
}
