/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#ifdef ENABLE_VAAPI

#include "ipreproc.hpp"

#include <memory_type.hpp>

#include <memory>

namespace InferenceBackend {
class VaApiConverter;
class VaApiImagePool;
class VaApiImage;
class VaApiContext;
} // namespace InferenceBackend

class VaapiPreProc : public IPreProc {
  public:
    VaapiPreProc(GstVideoInfo *input_video_info, const TensorCaps &output_tensor_info,
                 InferenceBackend::FourCC format = InferenceBackend::FourCC::FOURCC_RGBP,
                 InferenceBackend::MemoryType out_memory_type = InferenceBackend::MemoryType::SYSTEM);
    ~VaapiPreProc();

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer) final;
    void process(GstBuffer *buffer) final;

  private:
    GstVideoInfo *_input_video_info;
    TensorCaps _output_tensor_info;
    InferenceBackend::MemoryType _out_memory_type;

    std::unique_ptr<InferenceBackend::VaApiContext> _va_context;
    std::unique_ptr<InferenceBackend::VaApiConverter> _va_converter;
    std::shared_ptr<InferenceBackend::VaApiImagePool> _va_image_pool;
};

#endif // ENABLE_VAAPI