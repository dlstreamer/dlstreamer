/*******************************************************************************
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "model_proc_schema.h"

class ModelProcParserV2_2 : public ModelProcParserV2_1 {
  protected:
    void parseLayerNameAndFormat(ModelInputProcessorInfo::Ptr preprocessor, const nlohmann::json &proc_item) final {
        const auto layer_name_default = MODEL_PROC_SCHEMA_V2_2.at("properties")
                                            .at("input_preproc")
                                            .at("items")
                                            .at("properties")
                                            .at("layer_name")
                                            .at("default");
        const auto format_default = MODEL_PROC_SCHEMA_V2_2.at("properties")
                                        .at("input_preproc")
                                        .at("items")
                                        .at("properties")
                                        .at("format")
                                        .at("default");

        preprocessor->layer_name = proc_item.value("layer_name", layer_name_default);
        preprocessor->format = proc_item.value("format", format_default);
    }
};
