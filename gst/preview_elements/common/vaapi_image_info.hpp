/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#ifdef ENABLE_VAAPI

#include <vaapi_images.h>

struct VaapiImageInfo {
    std::shared_ptr<InferenceBackend::VaApiImagePool> pool;
    InferenceBackend::VaApiImage *image;
    std::promise<void> sync;
};

#endif
