/*******************************************************************************
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "py_object_wrapper.h"

#include <gst/video/video.h>

class PythonCallback {
    PyObjectWrapper py_function;
    PyObjectWrapper py_frame_class;
    std::string module_name;
    GstCaps *caps_ptr;

  public:
    PythonCallback(const char *module_path, const char *class_name, const char *function_name, const char *args_string,
                   const char *kwargs_string);
    void SetCaps(GstCaps *caps);
    ~PythonCallback() = default;

    gboolean CallPython(GstBuffer *buf);
};

class PythonContextInitializer {
  public:
    PythonContextInitializer();
    ~PythonContextInitializer();
    void initialize();
    void extendPath(const std::string &module_path);

  private:
    PyGILState_STATE state;
    PyObject *sys_path;
};
