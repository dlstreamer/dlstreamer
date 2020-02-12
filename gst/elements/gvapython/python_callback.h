/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "py_object_wrapper.h"

#include <array>
#include <gst/video/video.h>

class PythonCallback {
    PyObjectWrapper py_function;
    PyObjectWrapper py_videoframe_class;
    PyObjectWrapper py_caps;
    PyObjectWrapper py_class;

    std::string module_name;

  public:
    PythonCallback(const char *module_path, const char *class_name, const char *function_name, const char *arg_string,
                   GstCaps *caps);
    ~PythonCallback();

    void CallPython(GstBuffer *buf);
};
