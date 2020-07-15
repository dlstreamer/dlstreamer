/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_parser.h"

class ModelProcParserV2 : public ModelProcParser {
  public:
    virtual std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(const nlohmann::json &input_preproc) {
        std::vector<ModelInputProcessorInfo::Ptr> preproc_desc;

        for (const auto &proc_item : input_preproc) {
            std::shared_ptr<ModelInputProcessorInfo> preprocessor(new ModelInputProcessorInfo);
            preprocessor->layer_name = proc_item.at("layer_name");
            preprocessor->format = proc_item.at("format");

            preprocessor->params = gst_structure_new_empty("params");
            if (!preprocessor->params)
                std::throw_with_nested(std::runtime_error("Failed to allocate input preprocessing params"));
            nlohmann::basic_json<> preproc_params =
                JsonReader::getValueDefaultIfNotFound(proc_item, "params", nlohmann::basic_json<>());
            for (json::iterator it = preproc_params.begin(); it != preproc_params.end(); ++it) {
                std::string key = it.key();
                auto value = it.value();
                GValue gvalue = JsonReader::convertToGValue(value);
                gst_structure_set_value(preprocessor->params, key.data(), &gvalue);
                g_value_unset(&gvalue);
            }

            preproc_desc.push_back(preprocessor);
        }

        return preproc_desc;
    }
};
