/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "so_loader.h"

#include "inference_backend/logger.h"

#ifdef __linux__
#include <dlfcn.h>
#endif

#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
constexpr auto UNKNOWN_ERROR_MSG = "Unknown error";
constexpr auto WINDOWS_ERROR_MSG = "Loading shared objects is not implemented for Windows";
} // namespace

SharedObject::SharedObject(const std::string &library_name, int flags) {
#ifdef __linux__
    _handle = dlopen(library_name.c_str(), flags);
    if (!_handle) {
        auto dlerror_msg = dlerror();
        std::string msg = "Could not open shared object " + library_name +
                          " error: " + std::string(dlerror_msg ? dlerror_msg : UNKNOWN_ERROR_MSG);
        throw std::runtime_error(msg);
    }
#else
    throw std::runtime_error(WINDOWS_ERROR_MSG);
#endif
}

const SharedObject::Ptr &SharedObject::getLibrary(const std::string &library_name, int flags) {
    static std::map<std::string, SharedObject::Ptr> storage;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto it = storage.find(library_name);
    if (it == storage.end())
        it = storage.insert(it, {library_name, SharedObject::Ptr(new SharedObject(library_name, flags))});
    return it->second;
}

const SharedObject::Ptr &SharedObject::getLibrary(const std::string &library_name) {
#ifdef __linux__
    return getLibrary(library_name, RTLD_LAZY);
#else
    throw std::runtime_error(WINDOWS_ERROR_MSG);
#endif
}

SharedObject::~SharedObject() {
#ifdef __linux__
    if (_handle) {
        int result = dlclose(_handle);
        if (result != 0) {
            auto dlerror_msg = dlerror();
            GVA_WARNING("Could not close shared object: %s", dlerror_msg ? dlerror_msg : UNKNOWN_ERROR_MSG);
        }
    }
#endif
}

void *SharedObject::getSOFunction(void *so_handle, const std::string &function_name, std::string &error_msg) {
#ifdef __linux__
    error_msg.clear();
    void *func = dlsym(so_handle, function_name.c_str());
    if (!func) {
        auto dlerror_msg = dlerror();
        error_msg = dlerror_msg ? dlerror_msg : UNKNOWN_ERROR_MSG;
    }
    return func;
#else
    return nullptr;
#endif
}
