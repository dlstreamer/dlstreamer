/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "convert_tensor.h"

using json = nlohmann::json;

json convert_tensor(const GVA::Tensor &s_tensor) {
    json jobject = json::object();
    std::string precision_value = s_tensor.precision_as_string();
    if (!precision_value.empty()) {
        jobject.push_back(json::object_t::value_type("precision", precision_value));
    }
    std::string layout_value = s_tensor.layout_as_string();
    if (!layout_value.empty()) {
        jobject.push_back(json::object_t::value_type("layout", layout_value));
    }
    std::string name_value = s_tensor.name();
    if (!name_value.empty()) {
        jobject.push_back(json::object_t::value_type("name", name_value));
    }
    std::string model_name_value = s_tensor.model_name();
    if (!model_name_value.empty()) {
        jobject.push_back(json::object_t::value_type("model_name", model_name_value));
    }
    std::string layer_name_value = s_tensor.layer_name();
    if (!layer_name_value.empty()) {
        jobject.push_back(json::object_t::value_type("layer_name", layer_name_value));
    }
    std::string format_value = s_tensor.format();
    if (!format_value.empty()) {
        jobject.push_back(json::object_t::value_type("format", format_value));
    }
    if (!s_tensor.is_detection()) {
        std::string label_value = s_tensor.label();
        if (!label_value.empty()) {
            jobject.push_back(json::object_t::value_type("label", label_value));
        }
    }
    if (s_tensor.has_field("confidence")) {
        jobject.push_back(json::object_t::value_type("confidence", s_tensor.confidence()));
    }
    if (s_tensor.has_field("label_id")) {
        jobject.push_back(json::object_t::value_type("label_id", s_tensor.get_int("label_id")));
    }
    if (s_tensor.has_field("dims")) {
        json dims_array;

        const auto dims = s_tensor.dims();
        for (const auto &dim : dims)
            dims_array += dim;

        jobject.push_back(json::object_t::value_type("dims", dims_array));
    }

    json data_array;
    if (s_tensor.precision() == GVA::Tensor::Precision::U8) {
        const std::vector<uint8_t> data = s_tensor.data<uint8_t>();
        for (const auto &val : data) {
            data_array += val;
        }
    } else {
        const std::vector<float> data = s_tensor.data<float>();
        for (const auto &val : data) {
            data_array += val;
        }
    }
    if (!data_array.is_null()) {
        jobject.push_back(json::object_t::value_type("data", data_array));
    }

    return jobject;
}
