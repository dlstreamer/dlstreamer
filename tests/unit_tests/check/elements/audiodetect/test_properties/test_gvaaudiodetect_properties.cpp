/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "test_common.h"
#include "test_utils.h"

constexpr char plugin_name[] = "gvaaudiodetect";

static GstStaticPadTemplate srctemplate =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved"));

static GstStaticPadTemplate sinktemplate =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("audio/x-raw,format=S16LE,rate=16000,channels=1,layout=interleaved"));

GST_START_TEST(test_sliding_window_property_less_min) {
    g_print("Starting test: test_sliding_window_property_less_min\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_FLOAT);
    g_value_set_float(&prop_value, 0);

    check_property_default_if_invalid_value(plugin_name, "sliding-window", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_sliding_window_property_valid) {
    g_print("Starting test: test_sliding_window_property_valid\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_FLOAT);
    g_value_set_float(&prop_value, 0.5);

    check_property_value_updated_correctly(plugin_name, "sliding-window", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_sliding_window_property_higher_max) {
    g_print("Starting test: test_sliding_window_property_higher_max\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_FLOAT);
    g_value_set_float(&prop_value, 1.1);

    check_property_default_if_invalid_value(plugin_name, "sliding-window", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_sliding_window_property_string) {
    g_print("Starting test: test_sliding_window_property_string\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "101");

    check_property_default_if_invalid_value(plugin_name, "sliding-window", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_threshold_property_less_min) {
    g_print("Starting test: test_threshold_property_less_min\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_FLOAT);
    g_value_set_float(&prop_value, -0.1);

    check_property_default_if_invalid_value(plugin_name, "threshold", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_threshold_property_valid) {
    g_print("Starting test: test_threshold_property_valid\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_FLOAT);
    g_value_set_float(&prop_value, 0.7);

    check_property_value_updated_correctly(plugin_name, "threshold", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_device_property_valid) {
    g_print("Starting test: test_device_property_valid\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "GPU");

    check_property_value_updated_correctly(plugin_name, "device", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_fake_property) {
    g_print("Starting test: test_fake_property\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "GPU");

    check_property_value_updated_correctly(plugin_name, "fake_property", prop_value);
}

GST_END_TEST;
GST_START_TEST(test_threshold_property_higher_max) {
    g_print("Starting test: test_threshold_property_higher_max\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_FLOAT);
    g_value_set_float(&prop_value, 1.1);

    check_property_default_if_invalid_value(plugin_name, "threshold", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_threshold_property_string) {
    g_print("Starting test: test_threshold_property_higher_max\n");
    GValue prop_value = G_VALUE_INIT;
    g_value_init(&prop_value, G_TYPE_STRING);
    g_value_set_string(&prop_value, "0.5");

    check_property_default_if_invalid_value(plugin_name, "threshold", prop_value);
}

GST_END_TEST;

GST_START_TEST(test_model_property_invalid_path) {
    g_print("Starting test: test_model_property_invalid_path\n");
    std::string prop_value = "/a/non/existent/file.xml";
    std::string expected_msg = "path " + prop_value + " set in 'model' does not exist";

    check_multiple_property_init_fail_if_invalid_value(plugin_name, &srctemplate, &sinktemplate, expected_msg.c_str(),
                                                       "model", prop_value.c_str(), NULL);
}

GST_END_TEST;

GST_START_TEST(test_model_proc_property_invalid_path) {
    g_print("Starting test: test_model_proc_property_invalid_path\n");
    std::string prop_value = "/a/non/existent/file.json";
    std::string expected_msg = "does not exist"; //"Error loading json file: " + prop_value;

    char model_path[MAX_STR_PATH_SIZE];
    ExitStatus status = get_model_path(model_path, MAX_STR_PATH_SIZE, "lspeech_s5_ext", "FP32");
    ck_assert(status == EXIT_STATUS_SUCCESS);
    check_multiple_property_init_fail_if_invalid_value(plugin_name, &srctemplate, &sinktemplate, expected_msg.c_str(),
                                                       "model", model_path, "model-proc", prop_value.c_str(), NULL);
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

static Suite *audio_detection_properties_testing_suite(void) {
    Suite *s = suite_create("audio_detection_properties_testing_suite");
    TCase *tc_chain = tcase_create("general");

    suite_add_tcase(s, tc_chain);

    tcase_add_test(tc_chain, test_sliding_window_property_less_min);
    tcase_add_test(tc_chain, test_sliding_window_property_higher_max);
    tcase_add_test(tc_chain, test_sliding_window_property_string);
    tcase_add_test(tc_chain, test_sliding_window_property_valid);

    tcase_add_test(tc_chain, test_threshold_property_less_min);
    tcase_add_test(tc_chain, test_threshold_property_higher_max);
    tcase_add_test(tc_chain, test_threshold_property_string);
    tcase_add_test(tc_chain, test_threshold_property_valid);

    tcase_add_test(tc_chain, test_device_property_valid);

    tcase_add_test(tc_chain, test_model_property_invalid_path);

    // tcase_add_test(tc_chain, test_model_proc_property_invalid_path);

    tcase_add_test(tc_chain, test_qos_property_str_trash);
    tcase_add_test(tc_chain, test_fake_property);

    return s;
}

GST_CHECK_MAIN(audio_detection_properties_testing);
