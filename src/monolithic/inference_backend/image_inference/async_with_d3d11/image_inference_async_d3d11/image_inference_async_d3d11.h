/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "thread_pool.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"

namespace InferenceBackend {

class D3D11Converter;
class D3D11ImagePool;
class D3D11Image;
class D3D11Context;

class ImageInferenceAsyncD3D11 : public ImageInference {
  public:
    constexpr static size_t DEFAULT_THREAD_POOL_SIZE = 5;

    ImageInferenceAsyncD3D11(const InferenceBackend::InferenceConfig &config, dlstreamer::ContextPtr d3d11_context,
                             ImageInference::Ptr inference);

    ~ImageInferenceAsyncD3D11() override;

    void SubmitImage(IFrameBase::Ptr frame,
                     const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) override;

    const std::string &GetModelName() const override;

    size_t GetBatchSize() const override;
    size_t GetNireq() const override;

    void GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                int &memory_type) const override;

    std::map<std::string, std::vector<size_t>> GetModelInputsInfo() const override;
    std::map<std::string, std::vector<size_t>> GetModelOutputsInfo() const override;
    std::map<std::string, GstStructure *> GetModelInfoPostproc() const override;

    bool IsQueueFull() override;

    void Flush() override;

    void Close() override;

  private:
    // d3d11 image processing
    std::unique_ptr<D3D11Context> _d3d11_context;
    std::unique_ptr<D3D11Converter> _d3d11_converter;
    std::shared_ptr<D3D11ImagePool> _d3d11_image_pool;

    ImageInference::Ptr _inference;

    std::unique_ptr<D3D11::ThreadPool> _thread_pool;

    void SubmitInference(D3D11Image *d3d11_image, IFrameBase::Ptr frame,
                         const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors);
};

} // namespace InferenceBackend
