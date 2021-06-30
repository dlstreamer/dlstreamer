/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ipreproc.hpp"

#include <inference_backend/pre_proc.h>

class OpenCVPreProc : public IPreProc {
  public:
    OpenCVPreProc(GstVideoInfo *input_video_info, const TensorCaps &output_tensor_info,
                  const InferenceBackend::InputImageLayerDesc::Ptr &pre_proc_info = nullptr);

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer) final;
    void process(GstBuffer *buffer) final;

  private:
    GstVideoInfo *_input_video_info;
    TensorCaps _output_tensor_info;
    InferenceBackend::InputImageLayerDesc::Ptr _pre_proc_info;
};
