/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"

#include <inference_engine.hpp>

class OpenvinoBlobWrapper : public virtual InferenceBackend::Blob {
  public:
    OpenvinoBlobWrapper(InferenceEngine::Blob::Ptr blob) : blob(blob) {
    }

    const std::vector<size_t> &GetDims() const override {
        return blob->getTensorDesc().getDims();
    }

    InferenceBackend::Blob::Layout GetLayout() const override {
        return static_cast<InferenceBackend::Blob::Layout>((int)blob->getTensorDesc().getLayout());
    }

    InferenceBackend::Blob::Precision GetPrecision() const override {
        return static_cast<InferenceBackend::Blob::Precision>((int)blob->getTensorDesc().getPrecision());
    }

    virtual ~OpenvinoBlobWrapper() = default;

  protected:
    InferenceEngine::Blob::Ptr blob;
};

class OpenvinoInputBlob : public OpenvinoBlobWrapper, public InferenceBackend::InputBlob {
  public:
    OpenvinoInputBlob(InferenceEngine::Blob::Ptr blob) : OpenvinoBlobWrapper(blob), index(0) {
    }
    OpenvinoInputBlob(InferenceEngine::Blob::Ptr blob, size_t batch_index)
        : OpenvinoBlobWrapper(blob), index(batch_index) {
    }
    ~OpenvinoInputBlob() = default;
    void *GetData() override {
        return blob->buffer();
    }
    size_t GetIndexInBatch() const override {
        return index;
    }

  protected:
    size_t index;
};

class OpenvinoOutputBlob : public OpenvinoBlobWrapper, public InferenceBackend::OutputBlob {
  public:
    OpenvinoOutputBlob(InferenceEngine::Blob::Ptr blob) : OpenvinoBlobWrapper(blob) {
    }
    ~OpenvinoOutputBlob() = default;
    const void *GetData() const override {
        return blob->buffer();
    }
};
