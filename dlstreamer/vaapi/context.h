/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "dlstreamer/vaapi/buffer.h"

namespace dlstreamer {

class VAAPIContext : public Context {
  public:
    static constexpr auto context_name = "VAAPIContext";
    static constexpr auto va_display_id = "vaapi.display"; // VADisplay
    using VADisplay = void *;

    VAAPIContext(void *va_display) {
        _va_display = va_display;
    }

    VADisplay va_display() {
        return _va_display;
    }

    bool is_valid() noexcept {
        static constexpr int _VA_DISPLAY_MAGIC = 0x56414430; // #include <va/va_backend.h>
        return _va_display && (_VA_DISPLAY_MAGIC == *(int *)_va_display);
    }

    std::vector<std::string> keys() const override {
        return {va_display_id};
    }

    void *handle(std::string const &handle_id) const override {
        if (handle_id == va_display_id)
            return _va_display;
        return nullptr;
    }

  protected:
    VADisplay _va_display;
};

using VAAPIContextPtr = std::shared_ptr<VAAPIContext>;

} // namespace dlstreamer
