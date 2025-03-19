/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_parser.h"

class ModelProcParserV2_1 : public ModelProcParserV2 {
  protected:
    std::tuple<std::string, GstStructure *> parseProcessingItem(const nlohmann::basic_json<> &proc_item) override {
        const std::string def_layer_name = "ANY";
        std::string layer_name(def_layer_name);
        GstStructure *s = gst_structure_new_empty(layer_name.c_str());
        assert(s != nullptr);

        for (json::const_iterator it = proc_item.begin(); it != proc_item.end(); ++it) {
            std::string key = it.key();
            auto value = it.value();
            if (key == "attribute_name") {
                gst_structure_set_name(s, (static_cast<std::string>(value)).c_str());
                if (not gst_structure_has_name(s, (static_cast<std::string>(value)).c_str()))
                    throw std::invalid_argument("Not able to set name '" + static_cast<std::string>(value) +
                                                "' for GstStructure container for model-proc");
            }

            if (key == "layer_name") {
                if (layer_name != def_layer_name)
                    throw std::runtime_error("Has been maden attempt to overwrite layer_name.");

                layer_name = value;
            }
            if (key == "layer_names") {
                if (layer_name != def_layer_name)
                    throw std::runtime_error("Has been maden attempt to overwrite layer_name.");

                layer_name = "";
                for (const auto &el : value)
                    layer_name += static_cast<std::string>(el) + "\\";
                layer_name.pop_back();
            }

            GValue gvalue = JsonReader::convertToGValue(value);
            gst_structure_set_value(s, key.c_str(), &gvalue);
            g_value_unset(&gvalue);
        }
        if (layer_name == def_layer_name) {
            GST_WARNING("The 'layer_name' field has not been set. Its value will be defined as %s",
                        def_layer_name.c_str());
        }

        return std::make_tuple(layer_name, s);
    }
};
