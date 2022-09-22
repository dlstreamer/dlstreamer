/*******************************************************************************
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_parser.h"

class ModelProcParserV2 : public ModelProcParser {
  protected:
    void parseLayerNameAndFormat(ModelInputProcessorInfo::Ptr preprocessor, const nlohmann::json &proc_item) {
        preprocessor->layer_name = proc_item.at("layer_name");
        preprocessor->format = proc_item.at("format");
    }

  public:
    std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(const nlohmann::json &input_preproc) final {
        std::vector<ModelInputProcessorInfo::Ptr> preproc_desc;
        preproc_desc.reserve(input_preproc.size());

        for (const auto &proc_item : input_preproc) {
            ModelInputProcessorInfo::Ptr preprocessor = std::make_shared<ModelInputProcessorInfo>();

            parseLayerNameAndFormat(preprocessor, proc_item);

            preprocessor->precision = (preprocessor->format == "image") ? "U8" : "FP32";
            if (proc_item.find("precision") != proc_item.cend())
                preprocessor->precision = proc_item.at("precision");

            preprocessor->params = gst_structure_new_empty("params");
            if (!preprocessor->params)
                std::throw_with_nested(std::runtime_error("Failed to allocate input preprocessing params"));
            nlohmann::json preproc_params = proc_item.value("params", nlohmann::json());
            for (json::iterator it = preproc_params.begin(); it != preproc_params.end(); ++it) {
                const std::string &key = it.key();
                auto value = it.value();
                GValue gvalue = JsonReader::convertToGValue(value, key.c_str());
                gst_structure_set_value(preprocessor->params, key.c_str(), &gvalue);
                g_value_unset(&gvalue);
            }

            preproc_desc.push_back(preprocessor);
        }

        return preproc_desc;
    }
};
