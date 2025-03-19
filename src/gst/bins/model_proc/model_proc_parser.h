/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "input_model_preproc.h"

#include <gst/gst.h>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <tuple>

class ModelProcParser {
  protected:
    virtual std::tuple<std::string, GstStructure *> parseProcessingItem(const nlohmann::basic_json<> &);
    virtual void parseLayerNameAndFormat(ModelInputProcessorInfo::Ptr preprocessor,
                                         const nlohmann::json &proc_item) = 0;

  public:
    virtual std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(const nlohmann::json &input_preproc) = 0;
    virtual std::map<std::string, GstStructure *> parseOutputPostproc(const nlohmann::json &output_postproc);

    virtual ~ModelProcParser() = default;
};
