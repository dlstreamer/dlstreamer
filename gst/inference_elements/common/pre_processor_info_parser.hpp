/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/image_inference.h"

#include <gst/gst.h>

using PreProcResize = InferenceBackend::InputImageLayerDesc::Resize;
using PreProcCrop = InferenceBackend::InputImageLayerDesc::Crop;
using PreProcColorSpace = InferenceBackend::InputImageLayerDesc::ColorSpace;
using PreProcRangeNormalization = InferenceBackend::InputImageLayerDesc::RangeNormalization;
using PreProcDistribNormalization = InferenceBackend::InputImageLayerDesc::DistribNormalization;
using PreProcPadding = InferenceBackend::InputImageLayerDesc::Padding;

class PreProcParamsParser {
  private:
    const GstStructure *params;

    PreProcParamsParser() = delete;

    PreProcResize getResize() const;
    PreProcCrop getCrop() const;
    PreProcColorSpace getColorSpace() const;
    PreProcRangeNormalization getRangeNormalization() const;
    PreProcDistribNormalization getDistribNormalization() const;
    PreProcPadding getPadding() const;

  public:
    PreProcParamsParser(const GstStructure *params);
    InferenceBackend::InputImageLayerDesc::Ptr parse() const;
};
