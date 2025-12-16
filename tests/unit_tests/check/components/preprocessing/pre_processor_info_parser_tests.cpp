/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "common/pre_processor_info_parser.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

using namespace InferenceBackend;

const std::string range_field_name = "range";
const std::string mean_field_name = "mean";
const std::string std_field_name = "std";

TEST(PreProcParser, EmptyParamsField) {
    GstStructure *params = gst_structure_new_empty("preproc_model_params");
    PreProcParamsParser parser(params);

    InputImageLayerDesc::Ptr input_layer_desc = parser.parse();
    ASSERT_FALSE(input_layer_desc);

    gst_structure_free(params);
}

TEST(PreProcParser, DefaultValuesOfParamsField) {
    GstStructure *params = gst_structure_new("params", "color_space", G_TYPE_STRING, "RGB", NULL);

    PreProcParamsParser parser(params);

    InputImageLayerDesc::Ptr input_layer_desc = parser.parse();
    ASSERT_FALSE(input_layer_desc->doNeedResize());
    ASSERT_FALSE(input_layer_desc->doNeedCrop());
    ASSERT_TRUE(input_layer_desc->doNeedColorSpaceConversion(InputImageLayerDesc::ColorSpace::BGR));
    ASSERT_FALSE(input_layer_desc->doNeedRangeNormalization());
    ASSERT_FALSE(input_layer_desc->doNeedDistribNormalization());

    ASSERT_EQ(input_layer_desc->getResizeType(), InputImageLayerDesc::Resize::NO);
    ASSERT_EQ(input_layer_desc->getCropType(), InputImageLayerDesc::Crop::NO);
    ASSERT_EQ(input_layer_desc->getTargetColorSpace(), InputImageLayerDesc::ColorSpace::RGB);

    gst_structure_free(params);
}

TEST(PreProcParser, FullyFilledParamsField) {
    GstStructure *params = gst_structure_new("params", "resize", G_TYPE_STRING, "aspect-ratio", "crop", G_TYPE_STRING,
                                             "central", "color_space", G_TYPE_STRING, "RGB", NULL);
    GValueArray *arr = nullptr;
    std::vector<double> range = {1.0, 2.0};
    arr = ConvertVectorToGValueArr(range);
    gst_structure_set_array(params, range_field_name.c_str(), arr);

    std::vector<double> mean = {0.485, 0.456, 0.406};
    arr = ConvertVectorToGValueArr(mean);
    gst_structure_set_array(params, mean_field_name.c_str(), arr);

    std::vector<double> std = {0.229, 0.224, 0.225};
    arr = ConvertVectorToGValueArr(std);
    gst_structure_set_array(params, std_field_name.c_str(), arr);

    PreProcParamsParser parser(params);

    InputImageLayerDesc::Ptr input_layer_desc = parser.parse();
    ASSERT_TRUE(input_layer_desc->doNeedResize());
    ASSERT_TRUE(input_layer_desc->doNeedCrop());
    ASSERT_FALSE(input_layer_desc->doNeedColorSpaceConversion(InputImageLayerDesc::ColorSpace::RGB));
    ASSERT_TRUE(input_layer_desc->doNeedRangeNormalization());
    ASSERT_TRUE(input_layer_desc->doNeedDistribNormalization());

    ASSERT_EQ(input_layer_desc->getResizeType(), InputImageLayerDesc::Resize::ASPECT_RATIO);
    ASSERT_EQ(input_layer_desc->getCropType(), InputImageLayerDesc::Crop::CENTRAL);
    ASSERT_EQ(input_layer_desc->getTargetColorSpace(), InputImageLayerDesc::ColorSpace::RGB);

    ASSERT_DOUBLE_EQ(input_layer_desc->getRangeNormalization().min, range[0]);
    ASSERT_DOUBLE_EQ(input_layer_desc->getRangeNormalization().max, range[1]);

    compareArrays(input_layer_desc->getDistribNormalization().mean, mean);
    compareArrays(input_layer_desc->getDistribNormalization().std, std);

    g_value_array_free(arr);
    gst_structure_free(params);
}

TEST(PreProcParser, InvalidRangeGstStructure) {
    checkErrorThrowWithInvalidGstStructure(range_field_name);
    checkErrorThrowWithInvalidGstStructure(range_field_name, {0.0});
    checkErrorThrowWithInvalidGstStructure(range_field_name, {0.0, 1.0, 2.0});
}

TEST(PreProcParser, InvalidDistribNormalizationGstStructure) {
    GValueArray *g_arr = g_value_array_new(0);
    GstStructure *params = gst_structure_new_empty("params");
    gst_structure_set_array(params, mean_field_name.c_str(), g_arr);
    gst_structure_set_array(params, std_field_name.c_str(), g_arr);

    PreProcParamsParser parser(params);

    ASSERT_ANY_THROW(parser.parse());

    g_value_array_free(g_arr);
    gst_structure_free(params);
}
