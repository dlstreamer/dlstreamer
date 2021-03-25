/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "inference_backend/logger.h"
#include <dlfcn.h>
#include <functional>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <va/va_backend.h>

namespace internal {

/**
 * Class handles libva shared objects.
 * Has private methods to get/invoke functions from shared objects.
 * And has a public methods to call specific functions from specific shared objects.
 */
class VaApiLibBinderImpl {
  public:
    VaApiLibBinderImpl();
    VaApiLibBinderImpl(VaApiLibBinderImpl &) = delete;
    VaApiLibBinderImpl(VaApiLibBinderImpl &&) = delete;
    ~VaApiLibBinderImpl();

    VADisplay GetDisplayDRM(int file_descriptor);
    VAStatus Initialize(VADisplay dpy, int *major_version, int *minor_version);
    VAStatus Terminate(VADisplay dpy);
    std::function<const char *(VAStatus)> StatusToStrFunc();

  private:
    /**
     * Handles to dynamicly loaded libva & libva-drm.
     * Needed to call 'vaGetDisplayDRM' and 'vaInitialize'.
     * Further to invoke libva methods VADriverContext->vtable is used.
     */
    void *libva_handle = nullptr;
    void *libva_drm_handle = nullptr;

    /**
     * Loads the shared object using dlopen and returns the pointer to it's handle.
     *
     * @param[in] lib_name - name of a shared object.
     * @param[in] flags - flags for the dlopen (by default - RTLD_LAZY, resolves symbols only as the code that
     * references them is executed).
     *
     * @return handle for the opened shared object.
     *
     * @throw throw std::runtime_error when the error is occured during dlopen.
     */
    static void *load_shared_object(const std::string &lib_name, int flags = RTLD_LAZY);

    /**
     * Tries to close the shared object by the given handle using dlclose. Based on documentation decrements the
     * reference count on the dynamically loaded shared object referred to by handle, so the shared object closes only
     * when reference counter drops to zero.
     *
     * @param[in] handle - handle pointer to a shared object.
     */
    static void unload_shared_object(void *handle);

    /**
     * Finds the function named 'func_name' with given function prototype 'FuncProto' in given shared object 'handle'.
     * 'func_name' is not implied to ba a mangled name of a function (с++ functions in shared object must be wrapped
     * with 'extern C').
     *
     * If the 'handle' is null - the result dependps on 'dlsym' behavior.
     *
     * @param[in] handle - pointer to the shared object handle opened with dlopen.
     * @param[in] func_name - name of a function to be found.
     *
     * @return std::function<FuncProto> found function.
     *
     * @throw throw std::runtime_error when specified function is not found or handle is null.
     */
    template <typename FuncProto>
    static std::function<FuncProto> get_function(void *handle, const std::string &func_name) {
        if (handle == nullptr) {
            return nullptr;
        }

        auto func = reinterpret_cast<FuncProto *>(dlsym(handle, func_name.c_str()));
        if (func == nullptr) {
            std::string msg = "Could not load libva function: " + func_name + " " + std::string(dlerror());
            GVA_INFO(msg.c_str());
        }
        return func;
    }

    /**
     * Calls specified with 'func_name' function from given shared object's 'handle' with given 'args'.
     *
     * @param[in] handle - pointer to the shared object handle opened with dlopen.
     * @param[in] func_name - name of a function to be called.
     * @param[in] args - arguments pack to call the function.
     *
     * @return std::function<FuncProto>::result_type output type of an expected function.
     *
     * @throw throw std::runtime_error when specified function is not found or handle is null.
     */
    template <typename FuncProto, typename... Args>
    static auto invoke(void *handle, const std::string &func_name, Args &&... args) ->
        typename std::function<FuncProto>::result_type {

        auto func = get_function<FuncProto>(handle, func_name);
        return func(std::forward<Args>(args)...);
    }
};

} /* namespace internal */

/**
 * Wrapper around VaDisplay.
 * Needed for more convenient usage of VADisplay and its fields.
 */
class VaDpyWrapper final {
  public:
    explicit VaDpyWrapper() = default;
    explicit VaDpyWrapper(VADisplay d) : _dpy(d) {
        if (!enshure_display())
            throw std::invalid_argument("VADisplay is invalid.");
    }

    static VaDpyWrapper fromHandle(VADisplay d) {
        return VaDpyWrapper(d);
    }

    VADisplay raw() const noexcept {
        return _dpy;
    }

    explicit operator bool() const noexcept {
        return enshure_display();
    }

    VADisplayContextP dpyCtx() const noexcept {
        return reinterpret_cast<VADisplayContextP>(_dpy);
    }

    VADriverContextP drvCtx() const noexcept {
        return dpyCtx()->pDriverContext;
    }

    const VADriverVTable &drvVtable() const noexcept {
        return *drvCtx()->vtable;
    }

  private:
    VADisplay _dpy = nullptr;

    bool enshure_display() const noexcept {
        VADisplayContextP pDisplayContext = (VADisplayContextP)_dpy;
        return _dpy && pDisplayContext && (pDisplayContext->vadpy_magic == VA_DISPLAY_MAGIC) &&
               pDisplayContext->vaIsValid(pDisplayContext);
    }
};

/**
 * Singleton class to handle VaApiLibBinderImpl.
 * Multiple instances of VaApiLibBinderImpl is not needed.
 * Static modifier is used to simplify the usage and avoid crutches.
 */
class VaApiLibBinder {
  public:
    /* access method */
    static internal::VaApiLibBinderImpl &get();

  private:
    /* private constructor to prevent instantiation */
    VaApiLibBinder();
};

/* function loaded from libva to represent VAStatus as string */
static const std::function<const char *(VAStatus)> status_to_string = VaApiLibBinder::get().StatusToStrFunc();

#define VA_CALL(_FUNC)                                                                                                 \
    {                                                                                                                  \
        ITT_TASK(#_FUNC);                                                                                              \
        VAStatus _status = _FUNC;                                                                                      \
        if (_status != VA_STATUS_SUCCESS) {                                                                            \
            throw std::runtime_error(#_FUNC " failed, sts=" + std::to_string(_status) + " " +                          \
                                     status_to_string(_status));                                                       \
        }                                                                                                              \
    }
