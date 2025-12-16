/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "feature_toggling/ifeature_toggle.h"
#include "runtime_feature_toggler.h"

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

CREATE_FEATURE_TOGGLE(MyCoolFeatureToggle, "ENABLE_MYCOOL_FEATURES", "Some garbage imposed by developers")

CREATE_FEATURE_TOGGLE(MyinactiveFeatureToggle, "ENABLE_MYINACTIVE_FEATURES", "Some garbage imposed by developers")

struct Runtime_FT_Test : public ::testing::Test {

    FeatureToggling::Runtime::RuntimeFeatureToggler toggler;
};

TEST_F(Runtime_FT_Test, runtime_feature_toggling_test_enable_options) {
    toggler.configure({"ENABLE_MYCOOL_FEATURES"});
    ASSERT_TRUE(toggler.enabled(MyCoolFeatureToggle::id));
}

TEST_F(Runtime_FT_Test, runtime_feature_toggling_test_not_enable_options) {
    ASSERT_FALSE(toggler.enabled(MyinactiveFeatureToggle::id));
}
