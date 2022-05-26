/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <dlstreamer/context.h>

namespace dlstreamer {

class UsmContext : public Context {
  public:
    static constexpr auto ze_device_handle_id = "ze.handle.device";   // ze_device_handle_t
    static constexpr auto ze_context_handle_id = "ze.handle.context"; // ze_context_handle_t

    using ze_device_handle_t = struct _ze_device_handle_t *;
    using ze_context_handle_t = struct _ze_context_handle_t *;

    UsmContext(ze_device_handle_t ze_device_handle, ze_context_handle_t ze_context_handle)
        : _ze_device_handle(ze_device_handle), _ze_context_handle(ze_context_handle) {
    }

    ze_device_handle_t device_handle() const noexcept {
        return _ze_device_handle;
    }

    ze_context_handle_t context_handle() const noexcept {
        return _ze_context_handle;
    }

    void *handle(std::string const &handle_id) const override {
        if (handle_id == ze_device_handle_id)
            return _ze_device_handle;
        if (handle_id == ze_context_handle_id)
            return _ze_context_handle;
        return nullptr;
    }

    std::vector<std::string> keys() const override {
        return {ze_device_handle_id, ze_context_handle_id};
    }

  private:
    ze_device_handle_t _ze_device_handle;
    ze_context_handle_t _ze_context_handle;
};

} // namespace dlstreamer
