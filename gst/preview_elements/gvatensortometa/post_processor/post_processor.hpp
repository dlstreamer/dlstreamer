/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "converter_facade.hpp"
#include <capabilities/types.hpp>
#include <frame_data.hpp>

#include <map>
#include <memory>
#include <vector>

using ModelOutputsInfo = std::map<std::string, std::vector<size_t>>;

class PostProcessor {
  protected:
    std::vector<ConverterFacade> converters;

  public:
    enum class ExitStatus { SUCCESS, FAIL };
    enum class ModelProcOutputsValidationResult { OK, USE_DEFAULT, FAIL };

    /* parse output_postproc, create converters */
    PostProcessor(const TensorCaps &tensor_caps, const std::string &model_proc_path);

    ExitStatus process(GstBuffer *buffer, const TensorCaps &tensor_caps) const;

    PostProcessor() = default;
    ~PostProcessor() = default;

  private:
    ModelProcOutputsValidationResult
    validateModelProcOutputs(const std::map<std::string, GstStructure *> &model_proc_outputs,
                             const ModelOutputsInfo &model_outputs_info) const;
};
