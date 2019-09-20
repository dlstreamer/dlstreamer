/*******************************************************************************
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "vaapi_context.h"

#include "inference_backend/logger.h"

#include <cassert>
#include <tuple>

#include <fcntl.h>
#include <unistd.h>
#include <va/va.h>
#include <va/va_drm.h>

using namespace InferenceBackend;

namespace {

/*
std::tuple<VADisplay, int> create_va_display_and_device_descriptor() {
    int dri_file_descriptor = open("/dev/dri/renderD128", O_RDWR);
    if (!dri_file_descriptor) {
        throw std::runtime_error("Error opening /dev/dri/renderD128");
    }

    VADisplay display = vaGetDisplayDRM(dri_file_descriptor);
    if (!display) {
        throw std::runtime_error("Error opening VAAPI Display");
    }

    int major_version = 0, minor_version = 0;
    VA_CALL(vaInitialize(display, &major_version, &minor_version));

    return std::make_tuple(display, dri_file_descriptor);
}
*/

std::tuple<VAConfigID, VAContextID> create_config_and_context(VADisplay display) {
    if (!display) {
        throw std::invalid_argument("VADisplay is nullptr. Cannot initialize VaApiContext without VADisplay.");
    }
    VAConfigID config_id = 0;
    VA_CALL(vaCreateConfig(display, VAProfileNone, VAEntrypointVideoProc, nullptr, 0, &config_id));
    if (config_id == 0) {
        throw std::invalid_argument("Could not create VA config. Cannot initialize VaApiContext without VA config.");
    }
    VAContextID context_id = 0;
    VA_CALL(vaCreateContext(display, config_id, 0, 0, VA_PROGRESSIVE, nullptr, 0, &context_id));
    if (context_id == 0) {
        throw std::invalid_argument("Could not create VA context. Cannot initialize VaApiContext without VA context.");
    }
    return std::make_tuple(config_id, context_id);
}

} // namespace

VaApiContext::VaApiContext(MemoryType memory_type, VADisplay va_display)
    : _memory_type(memory_type), _va_display(va_display) {
    if (memory_type == MemoryType::VAAPI) {
        assert(_va_display != nullptr);
    } else {
        throw std::runtime_error("VaApiConverter: unsupported MemoryType");
    }

    std::tie(_va_config, _va_context_id) = create_config_and_context(_va_display);
}

VaApiContext::~VaApiContext() {
    if (_va_context_id != VA_INVALID_ID) {
        vaDestroyContext(_va_display, _va_context_id);
    }
    if (_va_config != VA_INVALID_ID) {
        vaDestroyConfig(_va_display, _va_config);
    }
    if (_va_display && _own_va_display) {
        VAStatus status = vaTerminate(_va_display);
        if (status != VA_STATUS_SUCCESS) {
            std::string error_message =
                std::string("VA Display termination failed with code ") + std::to_string(status);
            GVA_WARNING(error_message.c_str());
        }
        int status_code = close(_dri_file_descriptor);
        if (status_code != 0) {
            std::string error_message =
                std::string("DRI file descriptor closing failed with code ") + std::to_string(status_code);
            GVA_WARNING(error_message.c_str());
        }
    }
}

VAContextID VaApiContext::Id() {
    return _va_context_id;
}

VADisplay VaApiContext::Display() {
    return _va_display;
}
MemoryType VaApiContext::GetMemoryType() {
    return _memory_type;
}
