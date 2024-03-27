/*******************************************************************************
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/dictionary.h"
#include "dlstreamer/frame_info.h"
#include "dlstreamer/tensor.h"

namespace dlstreamer {
using FrameVector = std::vector<FramePtr>;
class OpenVinoInference;

class OpenVinoBackend {
  public:
    using InferenceCompleteCallback = std::function<void(FramePtr, TensorVector)>;

    OpenVinoBackend(DictionaryCPtr params, FrameInfo &input_info);
    ~OpenVinoBackend();

    void infer_async(FrameVector frames, InferenceCompleteCallback complete_cb);

    const std::string &get_model_name() const;

    FrameInfo get_model_input() const;
    FrameInfo get_model_output() const;

    const std::vector<std::string> &get_model_input_names() const;
    const std::vector<std::string> &get_model_output_names() const;

    void flush();

  private:
    std::unique_ptr<OpenVinoInference> _impl;
};

} // namespace dlstreamer