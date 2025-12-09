/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <fstream>
#include <gst/gst.h>
#include <nlohmann/json-schema.hpp>
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

    static GValue convertToGValue(const nlohmann::json::reference value, const char *key = "jsonobject");
};
