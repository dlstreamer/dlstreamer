/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "test_common.h"

constexpr char plugin_name[] = "gvabboxregression";

GST_START_TEST(test_qos_property_str_trash) {
    g_print("Starting test: test_qos_property_str_trash\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "qos", prop_value);
}

GST_END_TEST;

static Suite *bboxregression_properties_testing_suite(void) {
    Suite *s = suite_create("bboxregression_properties_testing");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);
    tcase_add_test(tc_chain, test_qos_property_str_trash);

    return s;
}

GST_CHECK_MAIN(bboxregression_properties_testing);
