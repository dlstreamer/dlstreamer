/*******************************************************************************
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <glib.h>
#include <gst/gst.h>

typedef GstFlowReturn (*AppsinkNewSampleCb)(GstElement *sink, gpointer user_data);
typedef void (*CheckSampleBufCb)(GstBuffer *buffer, gpointer user_data);

typedef struct _AppsinkTestData {
    CheckSampleBufCb check_buf_cb;
    guint64 frame_count_limit;
    gpointer user_data;
} AppsinkTestData;

void launch_pipeline(GstElement *pipeline);
void completion_pipeline(GstElement *pipeline);

void check_run_pipeline(const char *pipeline_str, guint64 timeout);
void check_run_pipeline_with_appsink(const char *pipeline_str, guint64 timeout, const char **appsink_names,
                                     size_t appsink_count, AppsinkNewSampleCb cb, gpointer user_data);
void check_run_pipeline_with_appsink_default(const char *pipeline_str, guint64 timeout, const char **appsink_names,
                                             size_t appsink_count, AppsinkTestData *test_data);
