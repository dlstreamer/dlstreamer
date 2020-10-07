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
    PyObjectWrapper py_frame_class;
    PyObjectWrapper py_caps;
    PyObjectWrapper py_class;
    std::string module_name;

  public:
    PythonCallback(const char *module_path, const char *class_name, const char *function_name, const char *args_string,
                   const char *kwargs_string);
    void SetCaps(GstCaps *caps);
    ~PythonCallback();

    gboolean CallPython(GstBuffer *buf);
};
