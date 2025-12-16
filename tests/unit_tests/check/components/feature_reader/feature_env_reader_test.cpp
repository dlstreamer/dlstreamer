/*******************************************************************************
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "environment_variable_options_reader.h"

#include "gva_utils.h"

#include <test_common.h>
#include <test_utils.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <gst/check/gstcheck.h>
#include <gst/gst.h>

#include <utility>

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;

struct Feature_Reader_Test : public ::testing::Test {

    FeatureToggling::Runtime::EnvironmentVariableOptionsReader env_var_options_reader;
};

TEST_F(Feature_Reader_Test, environment_variable_options_reader) {
    setenv("ENABLE_MYCOOL_FEATURES", "my-cool-feature,my-another-feature", 1);
    std::vector<std::string> my_features = env_var_options_reader.read("ENABLE_MYCOOL_FEATURES");
    ASSERT_EQ(my_features[0], "my-cool-feature");
    ASSERT_EQ(my_features[1], "my-another-feature");
    ASSERT_EQ(my_features.size(), 2);
}
