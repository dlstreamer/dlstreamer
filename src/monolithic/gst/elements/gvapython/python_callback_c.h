/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "gstgvapython.h"
#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct PythonCallback PythonCallback;

gboolean set_python_callback_caps(struct PythonCallback *python_callback, GstCaps *caps);

PythonCallback *create_python_callback(const char *module_path, const char *class_name, const char *function_name,
                                       const char *args_string, const char *kwargs_string);
GstFlowReturn invoke_python_callback(GstGvaPython *gvapython, GstBuffer *buffer);
void delete_python_callback(struct PythonCallback *python_callback);
void log_python_error(GstGvaPython *gvapython, gboolean is_fatal);

void create_arguments(void **args, void **kwargs);
gboolean update_arguments(const char *argument, void **args);
gboolean update_keyword_arguments(const char *argument, void **kwargs);
gchar *get_arguments_string(void *args);
void delete_arguments(void *args);

G_END_DECLS
