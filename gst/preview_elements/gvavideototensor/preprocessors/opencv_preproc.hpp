/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ipreproc.hpp"

#include <capabilities/types.hpp>
#include <inference_backend/pre_proc.h>

class OpenCVPreProc : public IPreProc {
  public:
    OpenCVPreProc(GstVideoInfo *input_video_info, const TensorCaps &output_tensor_info,
                  const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info = nullptr);

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer, GstVideoRegionOfInterestMeta *roi = nullptr) final;

    void flush() final{};

    size_t output_size() const final;
    bool need_preprocessing() const final;

  private:
    GstVideoInfo *_input_video_info;
    TensorCaps _output_tensor_info;
    InferenceBackend::InputImageLayerDesc::Ptr _pre_proc_info;
};
