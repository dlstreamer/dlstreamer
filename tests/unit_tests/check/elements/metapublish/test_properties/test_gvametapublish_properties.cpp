/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gvametapublish.hpp>
#include <test_common.h>

constexpr char plugin_name[] = "gvametapublish";

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(VIDEO_CAPS_TEMPLATE_STRING));

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(VIDEO_CAPS_TEMPLATE_STRING));

GST_START_TEST(test_method_property_invalid_value) {
    g_print("Starting test: test_method_property_invalid_value\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "fake_method");

    check_property_default_if_invalid_value(plugin_name, "method", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_signal_handoffs_property_as_string) {
    g_print("Starting test: test_signal_handoffs_property_less_zero\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "signal-handoffs", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_max_connect_attempts_property_as_string) {
    g_print("Starting test: test_max_connect_attempts_property_as_string\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "max-connect-attempts", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_max_reconnect_interval_property_above_max) {
    g_print("Starting test: test_max_reconnect_interval_property_above_max\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_UINT);
    g_value_set_uint(&prop_value, 301);

    check_property_default_if_invalid_value(plugin_name, "max-reconnect-interval", prop_value);
}

GST_END_TEST;

static Suite *metapublish_properties_testing_suite(void) {
    Suite *s = suite_create("metapublish_properties_testing");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);
    tcase_add_test(tc_chain, test_method_property_invalid_value);
    tcase_add_test(tc_chain, test_signal_handoffs_property_as_string);
    tcase_add_test(tc_chain, test_max_connect_attempts_property_as_string);
    tcase_add_test(tc_chain, test_max_reconnect_interval_property_above_max);

    return s;
}

GST_CHECK_MAIN(metapublish_properties_testing);
