/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>

#include "gva_json_meta.h"
#include "gva_tensor_meta.h"

GTEST_API_ int main(int argc, char **argv) {
    std::cout << "Running Components::GstVideoAnalyticsMeta Test from " << __FILE__ << std::endl;
    testing::InitGoogleTest(&argc, argv);
    gst_check_init(&argc, &argv);

    // register metadata
    gst_gva_json_meta_get_info();
    gst_gva_json_meta_api_get_type();
    gst_gva_tensor_meta_get_info();
    gst_gva_tensor_meta_api_get_type();

    return RUN_ALL_TESTS();
}
