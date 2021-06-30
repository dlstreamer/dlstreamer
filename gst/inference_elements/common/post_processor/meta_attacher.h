/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "post_proc_common.h"

#include "inference_backend/input_image_layer_descriptor.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#include <map>
#include <memory>
#include <string>

struct InferenceFrame;

namespace post_processing {
class MetaAttacher {
  protected:
    ModelImageInputInfo input_info;

  public:
    MetaAttacher(const ModelImageInputInfo &input_info) : input_info(input_info) {
    }

    virtual void attach(const TensorsTable &tensors_batch, InferenceFrames &frames) = 0;

    using Ptr = std::unique_ptr<MetaAttacher>;
    static Ptr create(int inference_type, int inference_region, const ModelImageInputInfo &input_image_info);

    virtual ~MetaAttacher() = default;
};

class ROIToFrameAttacher : public MetaAttacher {
  public:
    ROIToFrameAttacher(const ModelImageInputInfo &input_info) : MetaAttacher(input_info) {
    }

    void attach(const TensorsTable &tensors_batch, InferenceFrames &frames) override;
};
class TensorToFrameAttacher : public MetaAttacher {
  public:
    TensorToFrameAttacher(const ModelImageInputInfo &input_info) : MetaAttacher(input_info) {
    }

    void attach(const TensorsTable &tensors_batch, InferenceFrames &frames) override;
};
class TensorToROIAttacher : public MetaAttacher {
  private:
    GstVideoRegionOfInterestMeta *findROIMeta(GstBuffer *buffer, const GstVideoRegionOfInterestMeta &frame_roi);

  public:
    TensorToROIAttacher(const ModelImageInputInfo &input_info) : MetaAttacher(input_info) {
    }

    void attach(const TensorsTable &tensors_batch, InferenceFrames &frames) override;
};

} // namespace post_processing
