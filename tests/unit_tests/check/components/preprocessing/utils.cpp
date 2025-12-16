/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "utils.hpp"

#include "common/pre_processor_info_parser.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

GValueArray *ConvertVectorToGValueArr(const std::vector<double> &vector) {
    GValueArray *g_arr = g_value_array_new(vector.size());
    if (not g_arr)
        throw std::runtime_error("Failed to create GValueArray with " + std::to_string(vector.size()) + " elements");

    try {
        GValue gvalue = G_VALUE_INIT;
        g_value_init(&gvalue, G_TYPE_DOUBLE);
        for (guint i = 0; i < vector.size(); ++i) {
            g_value_set_double(&gvalue, static_cast<double>(vector[i]));
            g_value_array_append(g_arr, &gvalue);
        }

        return g_arr;
    } catch (const std::exception &e) {
        if (g_arr)
            g_value_array_free(g_arr);
        std::throw_with_nested(std::runtime_error("Failed to convert std::vector to GValueArray"));
    }
}

void compareArrays(const std::vector<double> &first, const std::vector<double> &second) {
    ASSERT_EQ(first.size(), second.size());

    for (size_t i = 0; i < first.size(); ++i)
        ASSERT_DOUBLE_EQ(first[i], second[i]);
}

void checkErrorThrowWithInvalidGstStructure(const std::string &field_name, const std::vector<double> invalid_arr) {
    GValueArray *g_arr = (invalid_arr.empty()) ? g_value_array_new(0) : ConvertVectorToGValueArr(invalid_arr);
    GstStructure *params = gst_structure_new_empty("params");
    gst_structure_set_array(params, field_name.c_str(), g_arr);

    PreProcParamsParser parser(params);

    ASSERT_ANY_THROW(parser.parse());

    g_value_array_free(g_arr);
    gst_structure_free(params);
}
