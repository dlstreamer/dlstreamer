/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "inference_backend/image_inference.h"

struct PostProcessor {
    enum class ExitStatus { SUCCESS, FAIL };

    virtual ExitStatus process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                               std::vector<std::shared_ptr<InferenceFrame>> &frames) = 0;
    virtual ~PostProcessor() = default;
};
