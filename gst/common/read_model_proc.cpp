/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "read_model_proc.h"
#include <fstream>
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;

GValue jsonvalue2gvalue(nlohmann::json::reference value) {
    GValue gvalue = G_VALUE_INIT;
    switch (value.type()) {
    case nlohmann::json::value_t::string:
        g_value_init(&gvalue, G_TYPE_STRING);
        g_value_set_string(&gvalue, ((std::string)value).data());
        break;
    case nlohmann::json::value_t::boolean:
        g_value_init(&gvalue, G_TYPE_BOOLEAN);
        g_value_set_boolean(&gvalue, (gboolean)value);
        break;
    case nlohmann::json::value_t::number_integer:
    case nlohmann::json::value_t::number_unsigned:
        g_value_init(&gvalue, G_TYPE_INT);
        g_value_set_int(&gvalue, (gint)value);
        break;
    case nlohmann::json::value_t::number_float:
        g_value_init(&gvalue, G_TYPE_DOUBLE);
        g_value_set_double(&gvalue, (gdouble)value);
        break;
    case nlohmann::json::value_t::array: {
        g_value_init(&gvalue, GST_TYPE_ARRAY);
        for (auto &el : value) {
            GValue a = jsonvalue2gvalue(el);
            gst_value_array_append_value(&gvalue, &a);
            g_value_unset(&a);
        }
        break;
    }
    case nlohmann::json::value_t::discarded:
    case nlohmann::json::value_t::null:
    case nlohmann::json::value_t::object:
        break;
    }
    return gvalue;
}

std::map<std::string, GstStructure *> ReadModelProc(std::string filepath) {
    std::ifstream input_file(filepath);
    if (!input_file) {
        throw std::runtime_error("Error loading json file: " + filepath + ", File was not found.");
    }
    json j;
    input_file >> j;
    input_file.close();
    std::map<std::string, GstStructure *> structures;
    for (int io = 0; io < 2; io++) {
        std::string io_name = io ? "output_postproc" : "input_preproc";
        for (auto &proc_item : j[io_name]) {
            std::string layer_name = "UNKNOWN";
            GstStructure *s = gst_structure_new_empty(layer_name.data());
            for (json::iterator it = proc_item.begin(); it != proc_item.end(); ++it) {
                std::string key = it.key();
                auto value = it.value();
                if (key == "attribute_name")
                    gst_structure_set_name(s, ((std::string)value).data());
                if (key == "layer_name")
                    layer_name = value;
                GValue gvalue = jsonvalue2gvalue(value);
                gst_structure_set_value(s, key.data(), &gvalue);
                g_value_unset(&gvalue);
            }
            if (!io) {
                gst_structure_set(s, "_is_preproc", G_TYPE_BOOLEAN, (gboolean)TRUE, NULL);
            }

            structures[layer_name] = s;
        }
    }
    return structures;
}

gboolean is_preprocessor(const GstStructure *processor) {
    return gst_structure_has_field(processor, "_is_preproc");
}
