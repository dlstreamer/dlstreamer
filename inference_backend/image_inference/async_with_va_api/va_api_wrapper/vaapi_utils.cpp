/*******************************************************************************
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <vaapi_utils.h>
namespace internal {

VaApiLibBinderImpl::VaApiLibBinderImpl() {
    _libva_so = SharedObject::getLibrary("libva.so.2");
    _libva_drm_so = SharedObject::getLibrary("libva-drm.so.2");
}

VADisplay VaApiLibBinderImpl::GetDisplayDRM(int file_descriptor) {
    auto dpy = _libva_drm_so->invoke<VADisplay(int)>("vaGetDisplayDRM", file_descriptor);
    if (!dpy) {
        throw std::runtime_error("Error opening VAAPI Display");
    }
    return dpy;
}

VAStatus VaApiLibBinderImpl::Initialize(VADisplay dpy, int *major_version, int *minor_version) {
    return _libva_so->invoke<VAStatus(VADisplay, int *, int *)>("vaInitialize", dpy, major_version, minor_version);
}

VAStatus VaApiLibBinderImpl::Terminate(VADisplay dpy) {
    return _libva_so->invoke<VAStatus(VADisplay)>("vaTerminate", dpy);
}

std::function<const char *(VAStatus)> VaApiLibBinderImpl::StatusToStrFunc() {
    return _libva_so->getFunction<const char *(VAStatus)>("vaErrorStr");
}

} /* namespace internal */

namespace {

static void message_callback_error(void * /*user_ctx*/, const char *message) {
    GVA_ERROR(message);
}

static void message_callback_info(void * /*user_ctx*/, const char *message) {
    GVA_INFO(message);
}

/**
 * Sets the internal VADisplay's error and info callbacks. Initializes the internal VADisplay.
 *
 * @pre _display must be set.
 * @pre libva_handle must be set to initialize VADisplay.
 * @pre message_callback_error, message_callback_info functions must reside in namespace to set callbacks.
 *
 * @throw std::runtime_error if the initialization failed.
 * @throw std::invalid_argument if display is null.
 */
void initializeVaDisplay(VaDpyWrapper display) {
    assert(display);

    display.dpyCtx()->error_callback = message_callback_error;
    display.dpyCtx()->error_callback_user_context = nullptr;

    display.dpyCtx()->info_callback = message_callback_info;
    display.dpyCtx()->info_callback_user_context = nullptr;

    int major_version = 0, minor_version = 0;
    VA_CALL(VaApiLibBinder::get().Initialize(display.raw(), &major_version, &minor_version));
}

} // namespace

VaApiDisplayPtr vaApiCreateVaDisplay(uint32_t relative_device_index) {
    constexpr uint32_t SYSTEM_DEV_ID = 128;
    std::string device_path = "/dev/dri/renderD" + std::to_string(SYSTEM_DEV_ID + relative_device_index);

    int dri_file_descriptor = open(device_path.c_str(), O_RDWR);
    // add errno
    if (!dri_file_descriptor) {
        throw std::runtime_error("Error opening " + device_path);
    }

    VADisplay display = VaApiLibBinder::get().GetDisplayDRM(dri_file_descriptor);
    initializeVaDisplay(VaDpyWrapper::fromHandle(display));

    auto deleter = [dri_file_descriptor](void *display) {
        VAStatus vastatus = VaApiLibBinder::get().Terminate(display);
        if (vastatus != VA_STATUS_SUCCESS) {
            auto error_message = std::string("VA Display termination failed with code ") + std::to_string(vastatus);
            GVA_WARNING(error_message.c_str())
        }
        int status = close(dri_file_descriptor);
        if (status != 0) {
            auto error_message = std::string("DRI file descriptor closing failed with code ") + std::to_string(status);
            GVA_WARNING(error_message.c_str())
        }
    };

    return VaApiDisplayPtr(display, deleter);
}

internal::VaApiLibBinderImpl &VaApiLibBinder::get() {
    static internal::VaApiLibBinderImpl instance;
    return instance;
}
