/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "post_proc_common.hpp"

#include "inference_backend/input_image_layer_descriptor.h"

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>

#include <map>
#include <memory>
#include <string>

struct InferenceFrame;

namespace PostProcessing {

class MetaAttacher {
  protected:
    static ModelImageInputInfo input_info;

  public:
    virtual void attach(const MetasTable &metas, GstBuffer *buffer) = 0;

    using Ptr = std::unique_ptr<MetaAttacher>;
    static Ptr create(const ModelImageInputInfo &input_image_info);

    virtual ~MetaAttacher() = default;
};

class TensorToFrameAttacher : public MetaAttacher {
  public:
    void attach(const MetasTable &metas, GstBuffer *buffer) override;
};

} // namespace PostProcessing
