/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <gst/check/gstcheck.h>
#include <stdio.h>

#include "pipeline_test_common.h"

void launch_pipeline(GstElement *pipeline) {
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    while (gst_element_get_state(pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_SUCCESS)
        continue;
    g_usleep(5e6);
}

void completion_pipeline(GstElement *pipeline) {
    gst_element_set_state(pipeline, GST_STATE_NULL);
    while (gst_element_get_state(pipeline, NULL, NULL, GST_CLOCK_TIME_NONE) != GST_STATE_CHANGE_SUCCESS)
        continue;
}

void check_run_pipeline(const char *pipeline_str, guint64 timeout) {
    GstElement *pipeline;

    GError *error = NULL;
    pipeline = gst_parse_launch(pipeline_str, &error);
    ck_assert(pipeline != NULL);
    ck_assert_msg(error == NULL, error->message);

    GstMessage *msg = NULL;
    GstBus *bus = gst_element_get_bus(pipeline);
    ck_assert(bus != NULL);

    launch_pipeline(pipeline);

    msg = gst_bus_timed_pop_filtered(bus, timeout, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    ck_assert(msg == NULL || GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS);
    if (msg) {
        gst_message_unref(msg);
        msg = NULL;
    }

    completion_pipeline(pipeline);

    if (bus) {
        gst_object_unref(bus);
        bus = NULL;
    }
    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = NULL;
    }
}

void check_run_pipeline_with_appsink(const char *pipeline_str, guint64 timeout, const char **appsink_names,
                                     size_t appsink_count, AppsinkNewSampleCb cb, gpointer user_data) {
    GstElement *pipeline = gst_parse_launch(pipeline_str, NULL);
    ck_assert(pipeline != NULL);

    for (int i = 0; i < appsink_count; i++) {
        GstElement *appsink = gst_bin_get_by_name(GST_BIN(pipeline), appsink_names[i]);
        ck_assert(appsink != NULL);

        g_object_set(appsink, "emit-signals", TRUE, NULL);
        gboolean emit_prop = FALSE;
        g_object_get(appsink, "emit-signals", &emit_prop, NULL);
        ck_assert(emit_prop);

        gulong sigid = g_signal_connect(appsink, "new-sample", G_CALLBACK(cb), user_data);
        ck_assert(sigid > 0);
    }

    GstBus *bus = gst_element_get_bus(pipeline);
    ck_assert(bus != NULL);

    launch_pipeline(pipeline);

    GstMessage *msg = gst_bus_timed_pop_filtered(bus, timeout, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    ck_assert(msg == NULL || GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS);
    if (msg) {
        gst_message_unref(msg);
        msg = NULL;
    }

    completion_pipeline(pipeline);

    if (bus) {
        gst_object_unref(bus);
        bus = NULL;
    }

    if (pipeline) {
        gst_object_unref(pipeline);
        pipeline = NULL;
    }
}

GstFlowReturn default_check_appsink_buffer(GstElement *sink, gpointer user_data) {
    static guint64 frame_counter = 0;
    frame_counter++;

    AppsinkTestData *data = (AppsinkTestData *)user_data;
    ck_assert(data != NULL);

    GstSample *sample;

    g_signal_emit_by_name(sink, "pull_sample", &sample);

    if (!sample)
        return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);

    if (data->check_buf_cb != NULL) {
        data->check_buf_cb(buffer, data->user_data);
    }

    if (sample) {
        gst_sample_unref(sample);
        sample = NULL;
    }

    if (data->frame_count_limit > 0 && frame_counter >= data->frame_count_limit) {
        gst_element_post_message(sink, gst_message_new_eos(GST_OBJECT(sink)));
        return GST_FLOW_EOS;
    }

    return GST_FLOW_OK;
}

void check_run_pipeline_with_appsink_default(const char *pipeline_str, guint64 timeout, const char **appsink_names,
                                             size_t appsink_count, AppsinkTestData *test_data) {
    check_run_pipeline_with_appsink(pipeline_str, timeout, appsink_names, appsink_count, default_check_appsink_buffer,
                                    test_data);
}
