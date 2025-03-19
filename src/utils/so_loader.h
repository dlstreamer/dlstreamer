/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <assert.h>

#include <functional>
#include <memory>
#include <stdexcept>

class SharedObject {
  public:
    using Ptr = std::shared_ptr<SharedObject>;

    /**
     * Tries to close the shared object handle using dlclose.
     */
    ~SharedObject();

    /**
     * Loads the shared object using dlopen or return SharedObject::Ptr from static storage if it is also loaded.
     *
     * @param[in] library_name - name of a shared object.
     * @param[in] flags - flags for the dlopen.
     *
     * @return SharedObject::Ptr for the opened shared object.
     *
     * @throw throw std::runtime_error when the error is occured during dlopen.
     */
    static const SharedObject::Ptr &getLibrary(const std::string &library_name, int flags);

    /**
     * Loads the shared object using getLibrary with flags = RTLD_LAZY.
     */
    static const SharedObject::Ptr &getLibrary(const std::string &library_name);

    /**
     * Finds the function named 'function_name' with given function prototype 'FuncProto' in shared object.
     *
     * @param[in] function_name - name of a function to be found.
     *
     * @return std::function<FuncProto> found function.
     *
     * @throw throw std::runtime_error when specified function is not found.
     */
    template <typename FuncProto>
    std::function<FuncProto> getFunction(const std::string &function_name) {
        assert(_handle && "shared object handle is null");

        std::string dl_error;
        auto loaded_function = reinterpret_cast<FuncProto *>(getSOFunction(_handle, function_name, dl_error));
        if (!loaded_function)
            throw std::runtime_error("Could not load function: " + function_name + " " + dl_error);

        return loaded_function;
    }

    /**
     * Calls function specified with 'function_name' from shared object with given 'args'.
     *
     * @param[in] function_name - name of a function to be called.
     * @param[in] args - arguments pack to call the function.
     *
     * @return std::function<FuncProto>::result_type output type of an expected function.
     *
     * @throw throw std::runtime_error when specified function is not found.
     */
    template <typename FuncProto, typename... Args>
    auto invoke(const std::string &function_name, Args &&...args) -> typename std::function<FuncProto>::result_type {
        auto func = getFunction<FuncProto>(function_name);
        return func(std::forward<Args>(args)...);
    }

    SharedObject(SharedObject &) = delete;
    SharedObject(SharedObject &&) = delete;
    void operator=(const SharedObject &) = delete;

  protected:
    SharedObject(const std::string &library_name, int flags);
    void *getSOFunction(void *so_handle, const std::string &function_name, std::string &error_msg);
    void *_handle = nullptr;
};
