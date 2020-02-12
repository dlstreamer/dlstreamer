/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct PythonCallback PythonCallback;

PythonCallback *create_python_callback(const char *module_path, const char *class_name, const char *function_name,
                                       const char *arg_string, GstCaps *caps);
gboolean invoke_python_callback(struct PythonCallback *python_callback, GstBuffer *buffer);
void delete_python_callback(struct PythonCallback *python_callback);
void log_python_error();

G_END_DECLS
