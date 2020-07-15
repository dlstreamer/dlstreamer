/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <fstream>
#include <gst/gst.h>
#include <json-schema.hpp>
#include <nlohmann/json.hpp>
#include <string>

using nlohmann::json_schema::json_validator;
using json = nlohmann::json;

class JsonReader {
  private:
    json_validator validator;
    json file_contents;

  public:
    void read(const std::string &file_path);
    void setSchema(const nlohmann::json &schema);
    void validate();
    const json &content() const;

    static GValue convertToGValue(const nlohmann::json::reference value);
    template <typename T>
    static T getValueDefaultIfNotFound(const nlohmann::basic_json<> &json_obj, const std::string &key, T default_value);
};

template <typename T>
T JsonReader::getValueDefaultIfNotFound(const nlohmann::basic_json<> &json_obj, const std::string &key,
                                        T default_value) {
    T result = default_value;
    auto iter = json_obj.find(key);
    if (iter != json_obj.end())
        result = iter.value();

    return result;
}
