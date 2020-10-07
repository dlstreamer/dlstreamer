/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include "processor_types.h"

class InferenceImpl;

namespace InferencePlugin {

class InferencePostProcessor : public PostProcessor {
  private:
    std::string model_name;

  public:
    InferencePostProcessor(const InferenceImpl *inference_impl);
    ExitStatus process(const std::map<std::string, InferenceBackend::OutputBlob::Ptr> &output_blobs,
                       std::vector<std::shared_ptr<InferenceFrame>> &frames) override;
};

} // namespace InferencePlugin
