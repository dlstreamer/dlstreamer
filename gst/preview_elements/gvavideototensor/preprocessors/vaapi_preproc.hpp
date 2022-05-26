/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"

#ifdef ENABLE_VAAPI

#include "ipreproc.hpp"

#include <capabilities/types.hpp>
#include <inference_backend/pre_proc.h>
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
    VaapiPreProc(VaApiDisplayPtr display, GstVideoInfo *input_video_info, const TensorCaps &output_tensor_info,
                 const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info = nullptr,
                 InferenceBackend::FourCC format = InferenceBackend::FourCC::FOURCC_RGBP,
                 InferenceBackend::MemoryType out_memory_type = InferenceBackend::MemoryType::SYSTEM);
    ~VaapiPreProc();

    void *displayRaw() const;

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer, GstVideoRegionOfInterestMeta *roi = nullptr) final;

    void flush() final;

    size_t output_size() const final;

    bool need_preprocessing() const final {
        return true;
    }

  private:
    GstVideoInfo *_input_video_info = nullptr;
    TensorCaps _output_tensor_info;
    InferenceBackend::InputImageLayerDesc::Ptr _pre_proc_info;
    InferenceBackend::MemoryType _out_memory_type;

    std::unique_ptr<InferenceBackend::VaApiContext> _va_context;
    std::unique_ptr<InferenceBackend::VaApiConverter> _va_converter;
    std::shared_ptr<InferenceBackend::VaApiImagePool> _va_image_pool;
};

#endif // ENABLE_VAAPI
