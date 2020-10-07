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

PyObject *extractClass(PyObjectWrapper &pluginModule, const char *class_name, const char *args_string,
                       const char *kwargs_string) {
    DECL_WRAPPER(class_type, PyObject_GetAttrString(pluginModule, class_name));

    if ((args_string) || (kwargs_string)) {
        DECL_WRAPPER(jsonModule, PyImport_ImportModule("json"));
        DECL_WRAPPER(json_load_s, PyObject_GetAttrString(jsonModule, "loads"));
        DECL_WRAPPER(tuple, PyTuple_New(0));
        DECL_WRAPPER(dict, PyDict_New());
        if (args_string) {
            DECL_WRAPPER(class_args, PyUnicode_FromString(args_string));
            DECL_WRAPPER(list, PyObject_CallFunctionObjArgs(json_load_s, (PyObject *)class_args, NULL));
            tuple.reset(PyList_AsTuple((PyObject *)list), "PyList_AsTuple((PyObject*)list)");
        }
        if (kwargs_string) {
            DECL_WRAPPER(class_args, PyUnicode_FromString(kwargs_string));
            dict.reset(PyObject_CallFunctionObjArgs(json_load_s, (PyObject *)class_args, NULL),
                       "PyObject_CallFunctionObjArgs(json_load_s,(PyObject*)class_args,NULL)");
        }
        return PyObject_Call(class_type, (PyObject *)tuple, (PyObject *)dict);
    }
    return PyObject_CallFunctionObjArgs(class_type, NULL);
}

gboolean callPython(GstBuffer *buffer, PyObjectWrapper &py_frame_class, PyObjectWrapper &py_caps,
                    PyObjectWrapper &py_function) {
    DECL_WRAPPER(py_buffer, pyg_boxed_new(buffer->mini_object.type, buffer, TRUE, TRUE));
    DECL_WRAPPER(frame, PyObject_CallFunctionObjArgs(py_frame_class, (PyObject *)py_buffer, Py_None,
                                                     (PyObject *)py_caps, nullptr));
    DECL_WRAPPER(args, Py_BuildValue("(O)", (PyObject *)frame));

    // make buffer writable, the reference counter was increased by pyg_boxed_new() call
    gst_buffer_unref(buffer);

    PyObjectWrapper result(PyObject_CallObject(py_function, args));

    // increase reference counter back, disposal of py_buffer object will decrease it
    gst_buffer_ref(buffer);
    if (((PyObject *)result) == nullptr) {
        throw std::runtime_error("Error in Python function");
    }
    return (PyObject_IsTrue(result) == 1) ? 1 : 0;
}

} // namespace

PythonCallback::PythonCallback(const char *module_path, const char *class_name, const char *function_name,
                               const char *args_string, const char *kwargs_string) {
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

    const char *extension = strrchr(module_path, '.');
    if (!extension) {
        module_name = std::string(filename);
    } else {
        module_name = std::string(filename, extension);
    }
    PyObjectWrapper pluginModule(PyImport_Import(WRAPPER(PyUnicode_FromString(module_name.c_str()))));
    if (!(PyObject *)pluginModule) {
        log_python_error(nullptr, false);
        throw std::runtime_error("Error loading Python module " + std::string(module_path));
    }
    if (class_name) {
        py_class.reset(extractClass(pluginModule, class_name, args_string, kwargs_string), "py_class");

        if (!(PyObject *)py_class) {
            log_python_error(nullptr, false);
            throw std::runtime_error("Error creating Python class " + std::string(class_name));
        }

        py_function.reset(PyObject_GetAttrString(py_class, function_name), "py_function");
    } else {
        py_function.reset(PyObject_GetAttrString(pluginModule, function_name), "py_function");
    }
    if (!(PyObject *)py_function) {
        throw std::runtime_error("Error getting function '" + std::string(function_name) + "' from Python module " +
                                 std::string(module_path));
    }
}

void PythonCallback::SetCaps(GstCaps *caps) {

    if (!(PyObject *)py_frame_class) {
        auto context_initializer = PythonContextInitializer();
        GstStructure *caps_s = gst_caps_get_structure((const GstCaps *)caps, 0);
        const gchar *name = gst_structure_get_name(caps_s);

        if (g_strrstr(name, "video")) {
            // Get gstgva.VideoFrame constructor
            DECL_WRAPPER(gva_module, PyImport_ImportModule("gstgva"));
            if (!py_frame_class.reset(PyObject_GetAttrString(gva_module, "VideoFrame"), "videoframe_class")) {
                throw std::runtime_error("Error getting gstgva.VideoFrame");
            }
        }
#ifdef AUDIO
        else if (g_strrstr(name, "audio")) {
            // Get gstgva.audio.AudioFrame constructor
            DECL_WRAPPER(gva_audio_module, PyImport_ImportModule("gstgva.audio"));
            if (!py_frame_class.reset(PyObject_GetAttrString(gva_audio_module, "AudioFrame"), "audioframe_class")) {
                throw std::runtime_error("Error getting gstgva.audio.AudioFrame");
            }
        }
#endif
        else {
            throw std::runtime_error("Invalid input caps");
        }
    }

    // Create Python caps
    if (!(PyObject *)py_caps) {
        if (!py_caps.reset(pyg_boxed_new(caps->mini_object.type, caps, TRUE, TRUE), "py_caps")) {
            throw std::runtime_error("Error creating Gst.Caps");
        }
    }
}

PythonCallback::~PythonCallback() {
    // TODO: when try to dealocate causes segfault
    py_class.release();
    py_caps.release();
    py_function.release();
    py_frame_class.release();
}

gboolean PythonCallback::CallPython(GstBuffer *buffer) {
    ITT_TASK(module_name.c_str());
    auto context_initializer = PythonContextInitializer();
    gboolean result = callPython(buffer, py_frame_class, py_caps, py_function);
    return result;
}