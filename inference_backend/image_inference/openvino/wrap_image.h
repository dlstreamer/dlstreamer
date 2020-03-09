/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"

#include <ie_blob.h>

InferenceEngine::Blob::Ptr WrapImageToBlob(const InferenceBackend::Image &image);
