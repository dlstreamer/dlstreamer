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

class VPUX : public General {
  public:
    VPUX(const InferenceEngine::RemoteContext::Ptr &remote_context);
    virtual ~VPUX() = default;

    virtual InferenceEngine::Blob::Ptr MakeSharedBlob(const InferenceBackend::Image &image,
                                                      const InferenceEngine::TensorDesc &tensor_desc,
                                                      size_t plane_num) const override;

  protected:
    const InferenceEngine::RemoteContext::Ptr &remote_context;
};

} // namespace WrapImageStrategy

InferenceEngine::Blob::Ptr WrapImageToBlob(const InferenceBackend::Image &image,
                                           const WrapImageStrategy::General &strategy);

#ifdef ENABLE_VAAPI
InferenceEngine::Blob::Ptr WrapImageToBlob(const InferenceBackend::Image &image,
                                           const InferenceEngine::RemoteContext::Ptr &remote_context);
#endif
