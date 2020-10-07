/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_proc_parser.h"
#include "inference_backend/logger.h"
#include "json_reader.h"

#include <cstring>

std::map<std::string, GstStructure *> ModelProcParser::parseOutputPostproc(const nlohmann::json &output_postproc) {
    std::map<std::string, GstStructure *> postproc_desc;
    std::string layer_name;
    GstStructure *s = nullptr;

    for (const auto &proc_item : output_postproc) {
        std::tie(layer_name, s) = parseProcessingItem(proc_item);
        auto iter = proc_item.find("converter");
        if (iter == proc_item.end()) {
            GVA_WARNING("The field 'converter' is not set");
        } else if (iter.value() == "") {
            GVA_WARNING("The value for field 'converter' is not set");
        }
        postproc_desc[layer_name] = s;
    }

    return postproc_desc;
}

std::tuple<std::string, GstStructure *> ModelProcParser::parseProcessingItem(const nlohmann::basic_json<> &proc_item) {
    const std::string def_layer_name = "ANY";
    std::string layer_name(def_layer_name);
    GstStructure *s = gst_structure_new_empty(layer_name.data());
    assert(s != nullptr);

    for (json::const_iterator it = proc_item.begin(); it != proc_item.end(); ++it) {
        std::string key = it.key();
        auto value = it.value();
        if (key == "attribute_name") {
            gst_structure_set_name(s, ((std::string)value).data());
            if (not gst_structure_has_name(s, ((std::string)value).data()))
                throw std::invalid_argument("Not able to set name '" + (std::string)value +
                                            "' for GstStructure container for model-proc");
        }
        if (key == "layer_name")
            layer_name = value;
        GValue gvalue = JsonReader::convertToGValue(value);
        gst_structure_set_value(s, key.data(), &gvalue);
        g_value_unset(&gvalue);
    }
    if (layer_name == def_layer_name) {
        const std::string msg = "\"layer_name\" field has not been set. Its value will be defined as " + def_layer_name;
        GVA_WARNING(msg.c_str());
    }

    return std::make_tuple(layer_name, s);
}
