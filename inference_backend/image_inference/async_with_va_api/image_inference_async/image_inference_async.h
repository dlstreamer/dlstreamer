/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#include "thread_pool.h"

#include "inference_backend/image_inference.h"
#include "inference_backend/logger.h"
#include "inference_backend/pre_proc.h"

#include <map>

namespace InferenceBackend {

class VaApiConverter;
class VaApiImagePool;
class VaApiImage;
class VaApiContext;

typedef void *VaApiDisplay;
class ImageInferenceAsync : public ImageInference {
  public:
    ImageInferenceAsync(int image_pool_size, MemoryType input_memory_type);

    ~ImageInferenceAsync() override;

    virtual void Init() override;

    void SubmitImage(const Image &image, IFrameBase::Ptr user_data,
                     const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors) override;

    const std::string &GetModelName() const override;
    void GetModelImageInputInfo(size_t &width, size_t &height, size_t &batch_size, int &format,
                                int &memory_type) const override;

    bool IsQueueFull() override;

    void Flush() override;

    void Close() override;

    VaApiDisplay GetVaDisplay() const;

    void SetInference(const ImageInference::Ptr &inference);

  private:
    // vaapi image processing
    std::unique_ptr<VaApiContext> _va_context;
    std::unique_ptr<VaApiConverter> _va_converter;
    std::shared_ptr<VaApiImagePool> _va_image_pool;

    ImageInference::Ptr _inference;

    ThreadPool _thread_pool;
    const size_t _VA_IMAGE_POOL_SIZE;

    void SubmitInference(VaApiImage *va_api_image, IFrameBase::Ptr user_data,
                         const std::map<std::string, InputLayerDesc::Ptr> &input_preprocessors);
};

} // namespace InferenceBackend
