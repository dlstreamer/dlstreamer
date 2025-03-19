/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_parser.h"

class ModelProcParserV2 : public ModelProcParser {
  protected:
    // Function to parse layer name and format from JSON and set them in the preprocessor object
    void parseLayerNameAndFormat(ModelInputProcessorInfo::Ptr preprocessor, const nlohmann::json &proc_item) {
        // Set the layer name from the JSON object
        preprocessor->layer_name = proc_item.at("layer_name");
        // Set the format from the JSON object
        preprocessor->format = proc_item.at("format");
    }

  public:
    // Function to parse input preprocessing information from JSON
    std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(const nlohmann::json &input_preproc) final {
        // Vector to hold the preprocessing descriptions
        std::vector<ModelInputProcessorInfo::Ptr> preproc_desc;
        preproc_desc.reserve(input_preproc.size());

        // Iterate over each item in the input preprocessing JSON array
        for (const auto &proc_item : input_preproc) {
            // Create a new ModelInputProcessorInfo object
            ModelInputProcessorInfo::Ptr preprocessor = std::make_shared<ModelInputProcessorInfo>();

            // Parse layer name and format from the JSON item
            parseLayerNameAndFormat(preprocessor, proc_item);

            // Set the precision based on the format
            preprocessor->precision = (preprocessor->format == "image") ? "U8" : "FP32";
            // If precision is specified in the JSON item, override the default precision
            if (proc_item.find("precision") != proc_item.cend())
                preprocessor->precision = proc_item.at("precision");

            // Create a new empty GStreamer structure for parameters
            preprocessor->params = gst_structure_new_empty("params");
            if (!preprocessor->params)
                std::throw_with_nested(std::runtime_error("Failed to allocate input preprocessing params"));

            // Get the parameters from the JSON item, defaulting to an empty JSON object if not found
            nlohmann::json preproc_params = proc_item.value("params", nlohmann::json());
            // Iterate over each parameter in the JSON object
            for (json::iterator it = preproc_params.begin(); it != preproc_params.end(); ++it) {
                const std::string &key = it.key(); // Get the parameter key
                auto value = it.value();           // Get the parameter value
                // Convert the JSON value to a GValue
                GValue gvalue = JsonReader::convertToGValue(value, key.c_str());
                gst_structure_set_value(preprocessor->params, key.c_str(), &gvalue);
                g_value_unset(&gvalue);
            }

            // Add the preprocessor object to the vector
            preproc_desc.push_back(preprocessor);
        }

        // Return the vector of preprocessing descriptions
        return preproc_desc;
    }
};