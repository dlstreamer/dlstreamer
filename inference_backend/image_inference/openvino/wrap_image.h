/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image.h"

#include <ie_blob.h>
#include <ie_remote_context.hpp>

namespace WrapImageStrategy {

class General {
  public:
    General() = default;
    virtual ~General() = default;

    virtual InferenceEngine::Blob::Ptr MakeSharedBlob(const InferenceBackend::Image &image,
                                                      const InferenceEngine::TensorDesc &tensor_desc,
                                                      size_t plane_num) const;
};

class Remote : public General {
  public:
    Remote(InferenceEngine::RemoteContext::Ptr remote_context) : _remote_context(remote_context) {
        if (!_remote_context)
            throw std::invalid_argument("Invalid remote context provided");
    }
    ~Remote() = default;

  protected:
    InferenceEngine::RemoteContext::Ptr _remote_context;
};

class VPUX : public Remote {
  public:
    VPUX(InferenceEngine::RemoteContext::Ptr remote_context) : Remote(remote_context) {
    }
    ~VPUX() = default;

    InferenceEngine::Blob::Ptr MakeSharedBlob(const InferenceBackend::Image &image,
                                              const InferenceEngine::TensorDesc &tensor_desc,
                                              size_t plane_num) const override;
};

class GPU : public Remote {
  public:
    GPU(InferenceEngine::RemoteContext::Ptr remote_context) : Remote(remote_context) {
    }
    ~GPU() = default;

    InferenceEngine::Blob::Ptr MakeSharedBlob(const InferenceBackend::Image &image, const InferenceEngine::TensorDesc &,
                                              size_t) const override;
};

} // namespace WrapImageStrategy

InferenceEngine::Blob::Ptr WrapImageToBlob(const InferenceBackend::Image &image,
                                           const WrapImageStrategy::General &strategy);
