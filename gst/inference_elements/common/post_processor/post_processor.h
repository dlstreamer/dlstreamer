/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "converter_facade.h"

#include "inference_backend/image_inference.h"

#include <map>
#include <memory>
#include <vector>

class InferenceImpl;
typedef struct _GvaBaseInference GvaBaseInference;

namespace post_processing {

class PostProcessor {
  protected:
    std::vector<ConverterFacade> converters;

  public:
    enum class ExitStatus { SUCCESS, FAIL };
    enum class ModelProcOutputsValidationResult { OK, USE_DEFAULT, FAIL };
    using ModelOutputsInfo = std::map<std::string, std::vector<size_t>>;

    PostProcessor(InferenceImpl *inference_impl,
                  GvaBaseInference *base_inference); // parse output_postproc, create converters

    ExitStatus process(const OutputBlobs &, InferenceFrames &) const;

    PostProcessor() = default;
    ~PostProcessor() = default;

  private:
    ModelProcOutputsValidationResult
    validateModelProcOutputs(const std::map<std::string, GstStructure *> &model_proc_outputs,
                             const ModelOutputsInfo &model_outputs_info) const;
    void setConverterIfNotSpecified(GstStructure *model_proc_output, const ModelOutputsInfo &model_outputs_info,
                                    int inference_type);
};
} // namespace post_processing
