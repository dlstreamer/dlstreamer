/*******************************************************************************
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "json_reader.h"
#include "model_proc_parser.h"

#include <string>

class ModelProcProvider {
  private:
    JsonReader json_reader;
    std::unique_ptr<ModelProcParser> model_proc_parser;

    void validateSchema(const nlohmann::json &json_schema);
    void createParser(const std::string &schema_version);

  public:
    void readJsonFile(const std::string &file_path);

    std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc();
    static std::vector<ModelInputProcessorInfo::Ptr> parseInputPreproc(std::map<std::string, GstStructure *>);
    std::map<std::string, GstStructure *> parseOutputPostproc();
};
