/*******************************************************************************
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "d3d11_context.h"
#include "d3d11_images.h"
#include "inference_backend/input_image_layer_descriptor.h"
#include <d3d11.h>
#include <mutex>
#include <windows.h>

#include <memory>

namespace InferenceBackend {

class D3D11Converter {
    D3D11Context *_context;

  protected:
    void SetupProcessorStreamsWithCustomParams(const InputImageLayerDesc::Ptr &pre_proc_info, uint16_t src_width,
                                               uint16_t src_height, uint16_t dst_width, uint16_t dst_height,
                                               RECT &src_rect, RECT &dst_rect,
                                               D3D11_VIDEO_PROCESSOR_STREAM &stream_params,
                                               const ImageTransformationParams::Ptr &image_transform_info);

  public:
    explicit D3D11Converter(D3D11Context *context);
    ~D3D11Converter() = default;

    void Convert(const Image &src, InferenceBackend::D3D11Image &d3d11_dst,
                 const InputImageLayerDesc::Ptr &pre_proc_info = nullptr,
                 const ImageTransformationParams::Ptr &image_transform_info = nullptr);
};

} // namespace InferenceBackend
