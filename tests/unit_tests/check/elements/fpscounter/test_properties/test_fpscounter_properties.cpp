/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "test_common.h"
#include "test_utils.h"

constexpr char plugin_name[] = "gvafpscounter";

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS(VIDEO_CAPS_TEMPLATE_STRING));

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS(VIDEO_CAPS_TEMPLATE_STRING));

GST_START_TEST(test_qos_property_str_trash) {
    g_print("Starting test: test_qos_property_str_trash\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "true");

    check_property_default_if_invalid_value(plugin_name, "qos", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_interval_property_letters) {
    g_print("Starting test: test_interval_property_letters\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "letters");

    check_property_default_if_invalid_value(plugin_name, "interval", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_interval_property_negative_number) {
    g_print("Starting test: test_interval_property_negative_number\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "-2");

    check_property_default_if_invalid_value(plugin_name, "interval", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_interval_property_reverse_negative_number) {
    g_print("Starting test: test_interval_property_reverse_negative_number\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "2-");

    check_property_default_if_invalid_value(plugin_name, "interval", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_interval_property_limit) {
    g_print("Starting test: test_interval_property_limit\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "9999999999");

    check_property_default_if_invalid_value(plugin_name, "interval", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_interval_property_trash) {
    g_print("Starting test: test_interval_property_trash\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "*&^%");

    check_property_default_if_invalid_value(plugin_name, "interval", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_interval_property_doublecomma_and_zeroend) {
    g_print("Starting test: test_interval_property_doublecomma_and_zeroend\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "9,,10");

    check_property_default_if_invalid_value(plugin_name, "interval", prop_value);
}

GST_END_TEST;

static Suite *fpscounter_properties_testing_suite(void) {
    Suite *s = suite_create("fpscounter_properties_testing");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);
    tcase_add_test(tc_chain, test_qos_property_str_trash);
    tcase_add_test(tc_chain, test_interval_property_letters);
    tcase_add_test(tc_chain, test_interval_property_negative_number);
    tcase_add_test(tc_chain, test_interval_property_reverse_negative_number);
    tcase_add_test(tc_chain, test_interval_property_limit);
    tcase_add_test(tc_chain, test_interval_property_trash);
    tcase_add_test(tc_chain, test_interval_property_doublecomma_and_zeroend);

    return s;
}

GST_CHECK_MAIN(fpscounter_properties_testing);
