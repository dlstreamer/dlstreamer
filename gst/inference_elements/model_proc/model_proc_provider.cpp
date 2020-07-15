/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "model_proc_provider.h"
#include "model_proc_parser_v1.h"
#include "model_proc_parser_v2.h"
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
        model_proc_parser = new ModelProcParserV1();
    } else if (schema_version == "2.0.0") {
        validateSchema(MODEL_PROC_SCHEMA_V2);
        model_proc_parser = new ModelProcParserV2();
    } else {
        std::string err_msg = "Parser for " + schema_version + " version not found";
        std::throw_with_nested(std::invalid_argument(err_msg));
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

std::map<std::string, GstStructure *> ModelProcProvider::parseOutputPostproc() {
    const nlohmann::json &model_proc_content = json_reader.content();
    return model_proc_parser->parseOutputPostproc(model_proc_content.at("output_postproc"));
}

ModelProcProvider::ModelProcProvider() : model_proc_parser(nullptr) {
}

ModelProcProvider::~ModelProcProvider() {
    delete model_proc_parser;
}
