/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"

#include "vaapi_utils.h"

#include <dlfcn.h>
#include <va/va_backend.h>

VADisplay createVASurface(VASurfaceID &surface_id, int &drm_fd);
InferenceBackend::Image createSurfaceImage(int &fd);
InferenceBackend::Image createEmptyImage();
