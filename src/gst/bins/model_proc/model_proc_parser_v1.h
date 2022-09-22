/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_parser.h"

class ModelProcParserV1 : public ModelProcParser {
  protected:
    void parseLayerNameAndFormat(ModelInputProcessorInfo::Ptr preprocessor, const nlohmann::json &proc_item) {
        preprocessor->layer_name = proc_item.value("layer_name", std::string("UNKNOWN"));
        preprocessor->format = proc_item.value("format", std::string("image"));
    }

  public:
    std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(const nlohmann::json &input_preproc) final {
        std::vector<ModelInputProcessorInfo::Ptr> preproc_desc;
        preproc_desc.reserve(input_preproc.size());

        for (const auto &proc_item : input_preproc) {
            ModelInputProcessorInfo::Ptr preprocessor = std::make_shared<ModelInputProcessorInfo>();

            parseLayerNameAndFormat(preprocessor, proc_item);

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
