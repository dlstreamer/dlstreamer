/*******************************************************************************
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_context.h"

#include "inference_backend/logger.h"

#include <cassert>
#include <tuple>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

using namespace InferenceBackend;

namespace {

static void message_callback_error(void * /*user_ctx*/, const char *message) {
    GVA_ERROR(message);
}

static void message_callback_info(void * /*user_ctx*/, const char *message) {
    GVA_INFO(message);
}

} // namespace

VaApiContext::VaApiContext(VADisplay va_display) : _display(va_display) {
    create_config_and_contexts();
    create_supported_pixel_formats();

    assert(_va_config_id != VA_INVALID_ID && "Failed to initalize VaApiContext. Expected valid VAConfigID.");
    assert(_va_context_id != VA_INVALID_ID && "Failed to initalize VaApiContext. Expected valid VAContextID.");
}

VaApiContext::VaApiContext() {
    create_va_display_and_device_descriptor();
    set_callbacks_and_initialize_va_display();
    _own_va_display = true;

    create_config_and_contexts();
    create_supported_pixel_formats();

    assert(_va_config_id != VA_INVALID_ID && "Failed to initalize VaApiContext. Expected valid VAConfigID.");
    assert(_va_context_id != VA_INVALID_ID && "Failed to initalize VaApiContext. Expected valid VAContextID.");
}

VaApiContext::~VaApiContext() {
    auto vtable = _display.drvVtable();
    auto ctx = _display.drvCtx();

    if (_va_context_id != VA_INVALID_ID) {
        vtable.vaDestroyContext(ctx, _va_context_id);
    }
    if (_va_config_id != VA_INVALID_ID) {
        vtable.vaDestroyConfig(ctx, _va_config_id);
    }
    if (_display && _own_va_display) {
        VAStatus status = VaApiLibBinder::get().Terminate(_display.raw());
        if (status != VA_STATUS_SUCCESS) {
            std::string error_message =
                std::string("VA Display termination failed with code ") + std::to_string(status);
            GVA_WARNING(error_message.c_str())
        }
        int status_code = close(_dri_file_descriptor);
        if (status_code != 0) {
            std::string error_message =
                std::string("DRI file descriptor closing failed with code ") + std::to_string(status_code);
            GVA_WARNING(error_message.c_str())
        }
    }
}

VAContextID VaApiContext::Id() const {
    return _va_context_id;
}

VaDpyWrapper VaApiContext::Display() const {
    return _display;
}

VADisplay VaApiContext::DisplayRaw() const {
    return _display.raw();
}

int VaApiContext::RTFormat() const {
    return _rt_format;
}

bool VaApiContext::IsPixelFormatSupported(int format) const {
    return _supported_pixel_formats.count(format);
}

/**
 * Creates VADisplay, sets the device descriptor and the VaDpyWrapper.
 *
 * @pre libva_drm_handle must be set to initialize VADisplay.
 * @post _dri_file_descriptor is set.
 * @post _display is set.
 *
 * @throw std::runtime_error if the initialization failed.
 * @throw std::invalid_argument if display is null.
 */
void VaApiContext::create_va_display_and_device_descriptor() {
    _dri_file_descriptor = open("/dev/dri/renderD128", O_RDWR);
    if (!_dri_file_descriptor) {
        throw std::runtime_error("Error opening /dev/dri/renderD128");
    }

    _display = VaDpyWrapper::fromHandle(VaApiLibBinder::get().GetDisplayDRM(_dri_file_descriptor));
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
void VaApiContext::set_callbacks_and_initialize_va_display() {
    assert(_display);

    _display.dpyCtx()->error_callback = message_callback_error;
    _display.dpyCtx()->error_callback_user_context = nullptr;

    _display.dpyCtx()->info_callback = message_callback_info;
    _display.dpyCtx()->info_callback_user_context = nullptr;

    int major_version = 0, minor_version = 0;
    VA_CALL(VaApiLibBinder::get().Initialize(_display.raw(), &major_version, &minor_version));
}

/**
 * Creates config, va context, and sets the driver context using the internal VADisplay.
 * Setting the VADriverContextP, VAConfigID, and VAContextID to the corresponding variables.
 *
 * @pre _display must be set and initialized.
 * @post _va_config_id is set.
 * @post _va_context_id is set.
 *
 * @throw std::invalid_argument if the VaDpyWrapper is not created, runtime format not supported, unable to get config
 * attributes, unable to create config, or unable to create context.
 */
void VaApiContext::create_config_and_contexts() {
    assert(_display);

    auto ctx = _display.drvCtx();
    auto vtable = _display.drvVtable();

    VAConfigAttrib format_attrib;
    format_attrib.type = VAConfigAttribRTFormat;
    VA_CALL(vtable.vaGetConfigAttributes(ctx, VAProfileNone, VAEntrypointVideoProc, &format_attrib, 1));
    if (not(format_attrib.value & _rt_format))
        throw std::invalid_argument("Could not create context. Runtime format is not supported.");

    VAConfigAttrib attrib;
    attrib.type = VAConfigAttribRTFormat;
    attrib.value = _rt_format;

    VA_CALL(vtable.vaCreateConfig(ctx, VAProfileNone, VAEntrypointVideoProc, &attrib, 1, &_va_config_id));
    if (_va_config_id == 0) {
        throw std::invalid_argument("Could not create VA config. Cannot initialize VaApiContext without VA config.");
    }
    VA_CALL(vtable.vaCreateContext(ctx, _va_config_id, 0, 0, VA_PROGRESSIVE, nullptr, 0, &_va_context_id));
    if (_va_context_id == 0) {
        throw std::invalid_argument("Could not create VA context. Cannot initialize VaApiContext without VA context.");
    }
}

/**
 * Creates a set of formats supported by image.
 *
 * @pre _display must be set and initialized.
 * @post _supported_pixel_formats is set.
 *
 * @throw std::runtime_error if vaQueryImageFormats return non success code
 */
void VaApiContext::create_supported_pixel_formats() {
    assert(_display);

    auto ctx = _display.drvCtx();
    auto vtable = _display.drvVtable();

    std::vector<VAImageFormat> image_formats(ctx->max_image_formats);
    int size = 0;
    VA_CALL(vtable.vaQueryImageFormats(ctx, image_formats.data(), &size));

    for (int i = 0; i < size; i++)
        _supported_pixel_formats.insert(image_formats[i].fourcc);
}
