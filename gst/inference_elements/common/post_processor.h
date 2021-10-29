/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/gst.h>

#include "post_processor/post_processor_impl.h"

#include <ie_layouts.h>

class InferenceImpl;
typedef struct _GvaBaseInference GvaBaseInference;

namespace post_processing {

struct RawBlob : public InferenceBackend::OutputBlob {
    using Ptr = std::shared_ptr<RawBlob>;

    const void *data;
    size_t byte_size;
    InferenceEngine::TensorDesc tensor_desc;

    RawBlob() = delete;
    ~RawBlob() final = default;

    RawBlob(const void *data, size_t byte_size, const InferenceEngine::TensorDesc &tensor_desc)
        : data(data), byte_size(byte_size), tensor_desc(tensor_desc) {
    }

    const std::vector<size_t> &GetDims() const final {
        return tensor_desc.getDims();
    }

    InferenceBackend::Blob::Layout GetLayout() const final {
        return static_cast<InferenceBackend::Blob::Layout>(static_cast<int>(tensor_desc.getLayout()));
    }

    InferenceBackend::Blob::Precision GetPrecision() const final {
        return static_cast<InferenceBackend::Blob::Precision>(static_cast<int>(tensor_desc.getPrecision()));
    }

    const void *GetData() const final {
        return data;
    }

    size_t GetByteSize() const {
        return byte_size;
    }
};

struct TensorDesc {
    std::string name;
    size_t size;
    InferenceEngine::TensorDesc ie_desc;

    TensorDesc(InferenceEngine::Precision precision, InferenceEngine::Layout layout, std::vector<size_t> dims,
               const std::string &layer_name, size_t tensor_size)
        : name(layer_name), size(tensor_size), ie_desc(InferenceEngine::TensorDesc(precision, dims, layout)) {
    }
};

class PostProcessor {
  public:
    enum class ModelProcOutputsValidationResult { OK, USE_DEFAULT, FAIL };

    PostProcessor(InferenceImpl *, GvaBaseInference *);
    PostProcessor(size_t image_width, size_t image_height, size_t batch_size, const std::string &model_proc_path,
                  const std::string &model_name, const ModelOutputsInfo &, ConverterType, double threshold);

    PostProcessorImpl::ExitStatus process(const OutputBlobs &, InferenceFrames &) const;
    PostProcessorImpl::ExitStatus process(GstBuffer *, const std::vector<TensorDesc> &,
                                          const std::string &instance_id) const;

  private:
    PostProcessorImpl post_proc_impl;
};

} /* namespace post_processing */
