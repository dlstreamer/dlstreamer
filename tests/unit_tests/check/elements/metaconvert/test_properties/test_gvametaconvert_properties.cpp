/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "test_common.h"

constexpr char plugin_name[] = "gvametaconvert";

GST_START_TEST(test_format_property_invalid_value) {
    g_print("Starting test: test_format_property_invalid_value\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "fake_format");

    check_property_default_if_invalid_value(plugin_name, "format", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_add_tensor_data_property_invalid_value) {
    g_print("Starting test: test_add_tensor_data_property_invalid_value\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "fake_add_tensor_data");

    check_property_default_if_invalid_value(plugin_name, "add-tensor-data", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_qos_property_str_trash) {
    g_print("Starting test: test_qos_property_str_trash\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "qos", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_signal_handoffs_property_less_zero) {
    g_print("Starting test: test_signal_handoffs_property_less_zero\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "signal-handoffs", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_add_empty_detection_results_property_str_trash) {
    g_print("Starting test: test_add_empty_detection_results_property_str_trash\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "add-empty-results", prop_value);
}

GST_END_TEST;

static Suite *metaconvert_properties_testing_suite(void) {
    Suite *s = suite_create("metaconvert_properties_testing");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);
    tcase_add_test(tc_chain, test_format_property_invalid_value);
    tcase_add_test(tc_chain, test_add_tensor_data_property_invalid_value);
    tcase_add_test(tc_chain, test_qos_property_str_trash);
    tcase_add_test(tc_chain, test_signal_handoffs_property_less_zero);
    tcase_add_test(tc_chain, test_add_empty_detection_results_property_str_trash);

    return s;
}

GST_CHECK_MAIN(metaconvert_properties_testing);