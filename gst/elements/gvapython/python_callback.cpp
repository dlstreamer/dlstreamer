/*******************************************************************************
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "python_callback.h"
#include "python_callback_c.h"

#include "gva_utils.h"
#include "inference_backend/logger.h"

#include <dlfcn.h>
#include <gmodule.h>
#include <pygobject-3.0/pygobject.h>

namespace {

class PythonContextInitializer {
    using PythonContext = std::pair<bool, PyGILState_STATE>;
    PythonContext context;
    PyObject *sys_path;

  public:
    PythonContextInitializer() {
        context = initContext();
        initPyGObject();

        sys_path = PySys_GetObject("path");

        // TODO: I'm not we need this
        extendPath(gvapythonModulePath());
    }
    ~PythonContextInitializer() {
        finalizeContext(context);
    }

    void extendPath(const std::string &module_path) {
        if (!module_path.empty()) {
            PyList_Append(sys_path, WRAPPER(PyUnicode_FromString(module_path.c_str())));
        }
    }

  private:
    PythonContext initContext() {
        bool initialized = Py_IsInitialized();
        PyGILState_STATE state = PyGILState_UNLOCKED;
        if (initialized) {
            state = PyGILState_Ensure();
        } else {
            Py_Initialize();
        }

        static wchar_t tmp[] = L"";
        static wchar_t *empty_argv[] = {tmp};
        PySys_SetArgv(1, empty_argv);

        return std::make_pair(initialized, state);
    }

    void finalizeContext(PythonContext &context) {
        if (context.first) {
            PyGILState_Release(context.second);
        } else {
            PyEval_SaveThread();
        }
    }

    void initPyGObject() {
        // load libpython.so and initilize pygobject
        Dl_info libpython_info = Dl_info();
        dladdr((void *)Py_IsInitialized, &libpython_info);
        GModule *libpython = g_module_open(libpython_info.dli_fname, G_MODULE_BIND_LAZY);
        if (!pygobject_init(3, 0, 0)) {
            throw std::runtime_error("pygobject_init failed");
        }
        if (libpython) {
            g_module_close(libpython);
        }
    }

    std::string gvapythonModulePath() {
        Dl_info dl_info = Dl_info();
        dladdr((void *)create_python_callback, &dl_info);
        const char *del = strrchr(dl_info.dli_fname, '/');
        if (del) {
            std::string dir(dl_info.dli_fname, del);
            dir += "/python";
            return dir;
        }
        return std::string();
    }
};

PyObject *extractClass(PyObjectWrapper &pluginModule, const char *class_name, const char *arg_string) {
    DECL_WRAPPER(class_type, PyObject_GetAttrString(pluginModule, class_name));
    if (arg_string) {
        DECL_WRAPPER(class_arg, PyUnicode_FromString(arg_string));
        return PyObject_CallFunctionObjArgs(class_type, (PyObject *)class_arg, NULL);
    }
    return PyObject_CallFunctionObjArgs(class_type, NULL);
}

const char *fileExtension(const char *filepath) {
    const char *extension = strrchr(filepath, '.');
    if (!extension) {
        extension = filepath + strlen(filepath);
    }
    return extension;
}

void callPython(GstBuffer *buffer, PyObjectWrapper &py_videoframe_class, PyObjectWrapper &py_caps,
                PyObjectWrapper &py_function) {
    DECL_WRAPPER(py_buffer, pyg_boxed_new(buffer->mini_object.type, buffer, TRUE, TRUE));
    DECL_WRAPPER(py_none, Py_None);
    DECL_WRAPPER(frame, PyObject_CallFunctionObjArgs(py_videoframe_class, (PyObject *)py_buffer, (PyObject *)py_none,
                                                     (PyObject *)py_caps, nullptr));
    DECL_WRAPPER(args, Py_BuildValue("(O)", (PyObject *)frame));

    // make buffer writable, the reference counter was increased by pyg_boxed_new() call
    gst_buffer_unref(buffer);

    PyObjectWrapper result(PyObject_CallObject(py_function, args));

    // increase reference counter back, disposal of py_buffer object will decrease it
    gst_buffer_ref(buffer);
    if (((PyObject *)result) == nullptr) {
        throw std::runtime_error("Could not call py function");
    }
}

} // namespace

PythonCallback::PythonCallback(const char *module_path, const char *class_name, const char *function_name,
                               const char *arg_string, GstCaps *caps) {
    ITT_TASK(__FUNCTION__);
    if (module_path == nullptr) {
        throw std::invalid_argument("module_path cannot be empty");
    }
    auto context_initializer = PythonContextInitializer();

    // add user-specified callback module into Python path
    const char *filename = strrchr(module_path, '/');
    if (filename) {
        std::string dir(module_path, filename);
        context_initializer.extendPath(dir);
        ++filename; // shifting next to '/'
    } else {
        filename = module_path;
    }
    module_name = std::string(filename, fileExtension(module_path));
    PyObjectWrapper pluginModule(PyImport_Import(WRAPPER(PyUnicode_FromString(module_name.c_str()))));
    if (!(PyObject *)pluginModule) {
        log_python_error();
        throw std::runtime_error("Error loading Python module " + std::string(module_path));
    }
    if (class_name) {
        py_class.reset(extractClass(pluginModule, class_name, arg_string), "py_class");
        py_function.reset(PyObject_GetAttrString(py_class, function_name), "py_function");
    } else {
        py_function.reset(PyObject_GetAttrString(pluginModule, function_name), "py_function");
    }
    if (!(PyObject *)py_function) {
        throw std::runtime_error("Error getting function '" + std::string(function_name) + "' from Python module " +
                                 std::string(module_path));
    }

    // Get gstgva.VideoFrame constructor
    DECL_WRAPPER(gva_module, PyImport_ImportModule("gstgva"));
    if (!py_videoframe_class.reset(PyObject_GetAttrString(gva_module, "VideoFrame"), "videoframe_class")) {
        throw std::runtime_error("Error getting gstgva.VideoFrame");
    }

    // Create Python caps
    if (!py_caps.reset(pyg_boxed_new(caps->mini_object.type, caps, TRUE, TRUE), "py_caps")) {
        throw std::runtime_error("Error creating Gst.Caps");
    }
}

PythonCallback::~PythonCallback() {
    // TODO: when try to dealocate causes segfault
    py_class.release();
    py_caps.release();
}

void PythonCallback::CallPython(GstBuffer *buffer) {
    ITT_TASK(module_name.c_str());

    PyGILState_STATE state = PyGILState_Ensure();

    callPython(buffer, py_videoframe_class, py_caps, py_function);

    PyGILState_Release(state);
}
