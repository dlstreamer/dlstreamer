/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "video_frame.h"

#include "copy_blob_to_gststruct.h"
#include <glib/gslice.h>
#include <gmock/gmock.h>
#include <gst/gstbuffer.h>
#include <gst/gstinfo.h>
#include <gst/gstmeta.h>
#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>

#include <gst/video/gstvideometa.h>

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

struct TensorTest : public ::testing::Test {

    // TODO: rename to SetUpTestSuite when migrate to googletest version higher than 1.8
    // https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#sharing-resources-between-tests-in-the-same-test-suite
    // static void SetUpTestCase() {
    // }

    GstStructure *structure;
    GVA::VideoFrame *frame;
    GVA::Tensor *tensor;

    void SetUp() {
        structure = gst_structure_new_empty("classification");
        uint8_t test_data[] = {0, 1, 2};
        const void *buffer = test_data;
        copy_buffer_to_structure(structure, buffer, 3);
        tensor = new GVA::Tensor(structure);
    }

    void TearDown() {
        if (structure)
            gst_structure_free(structure);
        if (tensor)
            delete tensor;
    }
};

TEST_F(TensorTest, TensorTestGetSet) {
    int test_rank = GVA_TENSOR_MAX_RANK;
    double test_confidence = 0.5;
    int test_obj_id = 1;
    int test_label_id = 2;

    GValueArray *test_array = g_value_array_new(test_rank);
    GValue gvalue = G_VALUE_INIT;
    g_value_init(&gvalue, G_TYPE_UINT);
    for (guint i = 0; i < test_rank; ++i) {
        g_value_set_uint(&gvalue, (guint)i);
        g_value_array_append(test_array, &gvalue);
    }

    ASSERT_EQ(tensor->name(), "classification");

    tensor->set_string("layer_name", "test_layer_name");
    ASSERT_TRUE(tensor->has_field("layer_name"));

    ASSERT_FALSE(tensor->is_detection());
    // "data_buffer" and "data" fields are already added by copy_buffer_to_structure call above
    ASSERT_EQ(tensor->fields(), std::vector<std::string>({"data_buffer", "data", "layer_name"}));

    tensor->set_string("model_name", "test_model_name");
    ASSERT_TRUE(tensor->has_field("model_name"));
    ASSERT_EQ(tensor->fields().size(), 4);

    tensor->set_string("element_id", "test_element_id");
    ASSERT_TRUE(tensor->has_field("element_id"));

    tensor->set_string("format", "test_format");
    ASSERT_TRUE(tensor->has_field("format"));

    tensor->set_string("label", "test_label");
    ASSERT_TRUE(tensor->has_field("label"));

    tensor->set_int("label_id", test_label_id);
    ASSERT_TRUE(tensor->has_field("label_id"));

    tensor->set_int("object_id", test_obj_id);
    ASSERT_TRUE(tensor->has_field("object_id"));

    tensor->set_int("precision", (int)GVA::Tensor::Precision::U8);
    ASSERT_TRUE(tensor->has_field("precision"));

    tensor->set_int("layout", (int)GVA::Tensor::Layout::NCHW);
    ASSERT_TRUE(tensor->has_field("layout"));

    tensor->set_int("rank", test_rank);
    ASSERT_TRUE(tensor->has_field("rank"));

    tensor->set_double("confidence", test_confidence);
    ASSERT_TRUE(tensor->has_field("confidence"));
    ASSERT_EQ(tensor->fields().size(), 13);

    ASSERT_EQ(tensor->get_double("confidence"), tensor->confidence());

    int precision = tensor->get_int("precision");
    ASSERT_EQ((GVA::Tensor::Precision)precision, tensor->precision());

    int layout = tensor->get_int("layout");
    ASSERT_EQ((GVA::Tensor::Layout)layout, tensor->layout());

    ASSERT_EQ(tensor->get_int("label_id"), tensor->label_id());

    ASSERT_EQ(tensor->get_string("element_id"), tensor->element_id());
    ASSERT_EQ(tensor->get_string("format"), tensor->format());
    ASSERT_EQ(tensor->get_string("label"), tensor->label());
    ASSERT_EQ(tensor->get_string("model_name"), tensor->model_name());
    ASSERT_EQ(tensor->get_string("layer_name"), tensor->layer_name());

    gst_structure_set_array(tensor->gst_structure(), "dims", test_array);
    ASSERT_TRUE(tensor->has_field("dims"));
    ASSERT_EQ(tensor->fields().size(), 14);

    std::vector<guint> dims = tensor->dims();
    int idx = 0;
    for (guint elem_dims : dims) {
        ASSERT_EQ(elem_dims, g_value_get_uint(g_value_array_get_nth(test_array, idx)));
        ++idx;
    }

    ASSERT_EQ(tensor->layout_as_string(), "NCHW");
    ASSERT_EQ(tensor->precision_as_string(), "U8");

    auto data_int = tensor->data<uint8_t>();
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(data_int[i], i);
    }

    float test_data[] = {0, 1, 2};
    const void *buffer = test_data;
    copy_buffer_to_structure(structure, buffer, 12);

    auto data_float = tensor->data<float>();
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(data_float[i], (float)i);
    }

    g_value_array_free(test_array);
}
