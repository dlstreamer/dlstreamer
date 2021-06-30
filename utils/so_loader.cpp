/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "so_loader.h"

#include "inference_backend/logger.h"

#include <dlfcn.h>

#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {
constexpr auto UNKNOWN_ERROR_MSG = "Unknown error";
} // namespace

SharedObject::SharedObject(const std::string &library_name, int flags) {
    _handle = dlopen(library_name.c_str(), flags);
    if (!_handle) {
        auto dlerr_msg = dlerror();
        std::string msg = "Could not open shared object " + library_name +
                          " error: " + std::string(dlerr_msg ? dlerr_msg : UNKNOWN_ERROR_MSG);
        throw std::runtime_error(msg);
    }
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
    return getLibrary(library_name, RTLD_LAZY);
}

SharedObject::~SharedObject() {
    if (_handle) {
        int result = dlclose(_handle);
        if (result != 0) {
            auto dlerr_msg = dlerror();
            std::string msg =
                "Could not close shared object: " + std::string(dlerr_msg ? dlerr_msg : UNKNOWN_ERROR_MSG);
            GVA_WARNING(msg.c_str());
        }
    }
}

void *SharedObject::getSOFunction(void *so_handle, const std::string &function_name, std::string &error_msg) {
    error_msg.clear();
    void *func = dlsym(so_handle, function_name.c_str());
    if (!func) {
        auto dlerr_msg = dlerror();
        error_msg = dlerr_msg ? dlerr_msg : UNKNOWN_ERROR_MSG;
    }
    return func;
}
