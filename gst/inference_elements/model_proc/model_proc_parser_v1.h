/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_parser.h"

class ModelProcParserV1 : public ModelProcParser {
  public:
    virtual std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(const nlohmann::json &input_preproc) {
        std::vector<ModelInputProcessorInfo::Ptr> preproc_desc;

        for (const auto &proc_item : input_preproc) {
            std::shared_ptr<ModelInputProcessorInfo> preprocessor(new ModelInputProcessorInfo);
            preprocessor->layer_name =
                JsonReader::getValueDefaultIfNotFound(proc_item, "layer_name", std::string("UNKNOWN"));
            preprocessor->format = JsonReader::getValueDefaultIfNotFound(proc_item, "format", std::string("image"));

            preprocessor->params = gst_structure_new_empty("params");

            for (json::const_iterator it = proc_item.begin(); it != proc_item.end(); ++it) {
                std::string key = it.key();
                if (key == "layer_name" || key == "format")
                    continue;

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
