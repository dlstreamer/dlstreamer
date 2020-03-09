/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "python_callback_c.h"

#include "python_callback.h"

#include "gva_utils.h"

void log_python_error() {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    if (pvalue) {
        PyObject *str = PyObject_Str(pvalue);
        if (str) {
            const char *error_message = PyUnicode_AsUTF8(str);
            if (error_message) {
                GST_ERROR("%s", error_message);
            }
        }
        PyErr_Restore(ptype, pvalue, ptraceback);
    }
}

PythonCallback *create_python_callback(const char *module_path, const char *class_name, const char *function_name,
                                       const char *arg_string, GstCaps *caps) {
    if (module_path == nullptr || function_name == nullptr || caps == nullptr) {
        GST_ERROR("module_path, function_name and caps must not be NULL");
        return nullptr;
    }
    try {
        return new PythonCallback(module_path, class_name, function_name, arg_string, caps);
    } catch (const std::exception &e) {
        GST_ERROR("%s", CreateNestedErrorMsg(e).c_str());
        return nullptr;
    }
}

gboolean invoke_python_callback(struct PythonCallback *python_callback, GstBuffer *buffer) {
    if (python_callback == nullptr) {
        GST_ERROR("python_callback is not initialized");
        return FALSE;
    }
    try {
        python_callback->CallPython(buffer);
        return TRUE;
    } catch (const std::exception &e) {
        GST_ERROR("%s", CreateNestedErrorMsg(e).c_str());
        log_python_error();
        return FALSE;
    }
}

void delete_python_callback(struct PythonCallback *python_callback) {
    if (python_callback != nullptr)
        delete python_callback;
}
