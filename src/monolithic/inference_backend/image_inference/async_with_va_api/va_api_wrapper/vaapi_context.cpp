/*******************************************************************************
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_context.h"

#include "inference_backend/logger.h"
#include "utils.h"

#include <cassert>
#include <vector>

#include <fcntl.h>

using namespace InferenceBackend;

VaApiContext::VaApiContext(VADisplay va_display) : _display(va_display) {
    create_config_and_contexts();
    create_supported_pixel_formats();

    assert(_va_config_id != VA_INVALID_ID && "Failed to initalize VaApiContext. Expected valid VAConfigID.");
    assert(_va_context_id != VA_INVALID_ID && "Failed to initalize VaApiContext. Expected valid VAContextID.");
}

VaApiContext::VaApiContext(dlstreamer::ContextPtr va_display_context)
    : _display_storage(va_display_context),
      _display(va_display_context->handle(dlstreamer::VAAPIContext::key::va_display)) {

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
