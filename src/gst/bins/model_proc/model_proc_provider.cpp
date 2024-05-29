/*******************************************************************************
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_proc_provider.h"
#include "model_proc_parser_v1.h"
#include "model_proc_parser_v2.h"
#include "model_proc_parser_v2_1.h"
#include "model_proc_parser_v2_2.h"
#include "model_proc_schema.h"

void ModelProcProvider::readJsonFile(const std::string &file_path) {
    json_reader.read(file_path);

    const nlohmann::json &model_proc_json = json_reader.content();
    auto it = model_proc_json.find("json_schema_version");
    if (it == model_proc_json.end())
        throw std::invalid_argument("Required property 'json_schema_version' not found in " + file_path +
                                    " model-proc file");

    const std::string schema_version = it.value();
    createParser(schema_version);
}

void ModelProcProvider::createParser(const std::string &schema_version) {
    // FIXME: maybe need to move schema validation in parser?
    if (schema_version == "1.0.0") {
        validateSchema(MODEL_PROC_SCHEMA_V1);
        model_proc_parser = std::make_unique<ModelProcParserV1>();
    } else if (schema_version == "2.0.0") {
        validateSchema(MODEL_PROC_SCHEMA_V2);
        model_proc_parser = std::make_unique<ModelProcParserV2>();
    } else if (schema_version == "2.1.0") {
        validateSchema(MODEL_PROC_SCHEMA_V2_1);
        model_proc_parser = std::make_unique<ModelProcParserV2_1>();
    } else if (schema_version == "2.2.0") {
        validateSchema(MODEL_PROC_SCHEMA_V2_2);
        model_proc_parser = std::make_unique<ModelProcParserV2_2>();
    } else {
        throw std::invalid_argument("Parser for " + schema_version + " version not found");
    }
}

void ModelProcProvider::validateSchema(const nlohmann::json &json_schema) {
    json_reader.setSchema(json_schema);
    json_reader.validate();
}

std::vector<ModelInputProcessorInfo::Ptr> ModelProcProvider::parseInputPreproc() {
    const nlohmann::json &model_proc_content = json_reader.content();
    return model_proc_parser->parseInputPreproc(model_proc_content.at("input_preproc"));
}

std::vector<ModelInputProcessorInfo::Ptr>
ModelProcProvider::parseInputPreproc(std::map<std::string, GstStructure *> info) {
    std::vector<ModelInputProcessorInfo::Ptr> preproc_desc;
    preproc_desc.reserve(info.size());
    for (const auto &item : info) {
        ModelInputProcessorInfo::Ptr preprocessor = std::make_shared<ModelInputProcessorInfo>();
        preprocessor->layer_name = item.first;
        preprocessor->format = std::string("image");
        preprocessor->precision = std::string("U8");
        preprocessor->params = item.second;
        preproc_desc.push_back(preprocessor);
    }

    return preproc_desc;
}

std::map<std::string, GstStructure *> ModelProcProvider::parseOutputPostproc() {
    const nlohmann::json &model_proc_content = json_reader.content();
    return model_proc_parser->parseOutputPostproc(model_proc_content.at("output_postproc"));
}
