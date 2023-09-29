/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/
#pragma once

#include <Python.h>
#include <stdexcept>

#include <gst/gst.h>

class PyObjectWrapper {
    PyObject *object;
    std::string description;

  public:
    PyObjectWrapper(PyObject *object = nullptr, const char *desc = nullptr) : object(object) {
        if (!object && desc) {
            throw std::runtime_error("Can't create PyObject " + std::string(desc));
        }
        description = desc != nullptr ? std::string(desc) : std::string();
    }

    operator PyObject *() {
        return object;
    }

    ~PyObjectWrapper() {
        if (object != nullptr) {
            GST_TRACE("~PyObjectWrapper() for %s", description.c_str());
        }
        Py_CLEAR(object);
    }

    PyObject *reset(PyObject *new_object = nullptr, const char *desc = nullptr) {
        description = desc != nullptr ? std::string(desc) : std::string();
        Py_CLEAR(object);
        object = new_object;
        return object;
    }

    PyObject *release() {
        PyObject *tmp = object;
        object = nullptr;
        return tmp;
    }

    PyObjectWrapper(const PyObjectWrapper &other) = delete;
    PyObjectWrapper &operator=(PyObjectWrapper other) = delete;
};

#define WRAPPER(_OBJECT) PyObjectWrapper(_OBJECT, #_OBJECT)

#define DECL_WRAPPER(_NAME, _OBJECT) PyObjectWrapper _NAME(_OBJECT, #_OBJECT)
