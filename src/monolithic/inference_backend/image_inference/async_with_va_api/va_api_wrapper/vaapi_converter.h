/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/input_image_layer_descriptor.h"

#include "vaapi_context.h"
#include "vaapi_images.h"
#include "vaapi_utils.h"

#include <memory>

namespace InferenceBackend {

class VaApiConverter {
    static std::mutex _convert_mutex;
    VaApiContext *_context;

  protected:
    void SetupPipelineRegionsWithCustomParams(const InputImageLayerDesc::Ptr &pre_proc_info, uint16_t src_width,
                                              uint16_t src_height, uint16_t dst_width, uint16_t dst_height,
                                              VARectangle &src_surface_region, VARectangle &dst_surface_region,
                                              VAProcPipelineParameterBuffer &pipeline_param,
                                              const ImageTransformationParams::Ptr &image_transform_info);

  public:
    explicit VaApiConverter(VaApiContext *context);
    ~VaApiConverter() = default;

    void Convert(const Image &src, InferenceBackend::VaApiImage &va_api_dst,
                 const InputImageLayerDesc::Ptr &pre_proc_info = nullptr,
                 const ImageTransformationParams::Ptr &image_transform_info = nullptr);
};

} // namespace InferenceBackend
