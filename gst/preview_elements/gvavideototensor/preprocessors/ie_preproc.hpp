/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "ipreproc.hpp"

#include <ie_preprocess.hpp>

class IEPreProc : public IPreProc {
  public:
    IEPreProc(GstVideoInfo *video_info);

    void process(GstBuffer *in_buffer, GstBuffer *out_buffer) final;
    void process(GstBuffer *buffer) final;

  private:
    int _channels;
    size_t _width;
    size_t _height;
    std::unique_ptr<InferenceEngine::PreProcessInfo> _pre_proc_info;
};
