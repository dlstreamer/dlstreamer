/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <vaapi_utils.h>

namespace internal {

VaApiLibBinderImpl::VaApiLibBinderImpl() {
    libva_handle = load_shared_object("libva.so");
    libva_drm_handle = load_shared_object("libva-drm.so");
}

VaApiLibBinderImpl::~VaApiLibBinderImpl() {
    unload_shared_object(libva_handle);
    unload_shared_object(libva_drm_handle);
}

VADisplay VaApiLibBinderImpl::GetDisplayDRM(int file_descriptor) {
    auto dpy = invoke<VADisplay(int)>(libva_drm_handle, "vaGetDisplayDRM", file_descriptor);
    if (!dpy) {
        throw std::runtime_error("Error opening VAAPI Display");
    }
    return dpy;
}

VAStatus VaApiLibBinderImpl::Initialize(VADisplay dpy, int *major_version, int *minor_version) {
    return invoke<VAStatus(VADisplay, int *, int *)>(libva_handle, "vaInitialize", dpy, major_version, minor_version);
}

VAStatus VaApiLibBinderImpl::Terminate(VADisplay dpy) {
    return invoke<VAStatus(VADisplay)>(libva_handle, "vaTerminate", dpy);
}

std::function<const char *(VAStatus)> VaApiLibBinderImpl::StatusToStrFunc() {
    return get_function<const char *(VAStatus)>(libva_handle, "vaErrorStr");
}

void *VaApiLibBinderImpl::load_shared_object(const std::string &lib_name, int flags) {
    void *handle = dlopen(lib_name.c_str(), flags);
    if (!handle) {
        std::string msg = "Could not open libva: " + std::string(dlerror());
        GVA_INFO(msg.c_str());
    }
    return handle;
}

void VaApiLibBinderImpl::unload_shared_object(void *lib_handle) {
    if (lib_handle != nullptr) {
        int result = dlclose(lib_handle);
        if (result != 0) {
            std::string msg = "Could not close libva: " + std::string(dlerror());
            GVA_INFO(msg.c_str());
        }
    }
}

} /* namespace internal */

internal::VaApiLibBinderImpl &VaApiLibBinder::get() {
    static internal::VaApiLibBinderImpl instance;
    return instance;
}
