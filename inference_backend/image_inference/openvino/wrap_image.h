/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"

#include <ie_blob.h>
#include <ie_remote_context.hpp>

InferenceEngine::Blob::Ptr WrapImageToBlob(const InferenceBackend::Image &image);

#ifdef ENABLE_VAAPI
InferenceEngine::Blob::Ptr WrapImageToBlob(const InferenceBackend::Image &image,
                                           const InferenceEngine::RemoteContext::Ptr &remote_context);
#endif
