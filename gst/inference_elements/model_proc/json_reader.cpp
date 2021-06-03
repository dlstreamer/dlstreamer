/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "json_reader.h"

void JsonReader::read(const std::string &file_path) {
    std::ifstream input_file(file_path);
    if (not input_file)
        std::throw_with_nested(std::runtime_error("Model-proc file '" + file_path + "' was not found"));

    input_file >> file_contents;
    input_file.close();
}

void JsonReader::setSchema(const nlohmann::json &schema) {
    try {
        validator.set_root_schema(schema);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to load model-proc schema"));
    }
}

void JsonReader::validate() {
    try {
        validator.validate(file_contents);
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Model-proc validation failed"));
    }
}

const json &JsonReader::content() const {
    return file_contents;
}

GValue JsonReader::convertToGValue(const nlohmann::json::reference value) {
    GValue gvalue = G_VALUE_INIT;
    try {
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
                GValue a = convertToGValue(el);
                gst_value_array_append_value(&gvalue, &a);
                g_value_unset(&a);
            }
            break;
        }
        // TODO: need to be implemented in case of nested dict
        case nlohmann::json::value_t::object: {
            g_value_init(&gvalue, GST_TYPE_STRUCTURE);
            json obj = (json)value;
            GstStructure *s = gst_structure_new_empty("jsonobject");
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                std::string key = it.key();
                auto value = it.value();
                GValue gvalue = JsonReader::convertToGValue(value);
                gst_structure_set_value(s, key.data(), &gvalue);
            }
            gst_value_set_structure(&gvalue, s);
            break;
            // g_value_init(&gvalue, G_TYPE_POINTER);
            // GHashTable *hash_table = g_hash_table_new(g_direct_hash, g_direct_equal);
            // for (auto it = value.begin(); it != value.end(); ++it) {
            //     auto key = it.key();
            //     GValue a = convertToGValue(it.value());

            //     g_hash_table_insert(hash_table, strdup(it.key()), (gpointer)to_json);
            //     g_hash_table_insert(hash_table, GINT_TO_POINTER(GST_GVA_METACONVERT_DUMP_DETECTION),
            //                         (gpointer)dump_detection);
            //     // gst_value_array_append_value(&gvalue, &a);
            //     // g_value_unset(&a);
            // }
            break;
        }
        case nlohmann::json::value_t::discarded:
        case nlohmann::json::value_t::null:
            break;
        }
    } catch (const std::exception &e) {
        std::throw_with_nested(std::runtime_error("Failed to create GValue from json value"));
    }
    return gvalue;
}
