/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"

#ifdef ENABLE_VAAPI

#include "ipreproc.hpp"
#include "vaapi_preproc.hpp"

#include <ie_preprocess.hpp>

using VaApiDisplayPtr = std::shared_ptr<void>;

class VaapiSurfaceSharingPreProc : public IPreProc {
  public:
    VaapiSurfaceSharingPreProc(VaApiDisplayPtr display, GstVideoInfo *input_video_info,
                               const TensorCaps &output_tensor_info);
    ~VaapiSurfaceSharingPreProc();

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer) final;
    void process(GstBuffer *buffer) final;

  private:
    GstVideoInfo *_input_video_info;
    TensorCaps _output_tensor_info;
    std::unique_ptr<InferenceEngine::PreProcessInfo> _pre_proc_info;
    std::unique_ptr<VaapiPreProc> _vaapi_pre_proc;
};

#endif
