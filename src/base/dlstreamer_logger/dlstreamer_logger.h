/*******************************************************************************
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <spdlog/spdlog.h>
#include <string>
#include <string_view>

namespace dlstreamer {

namespace itt {
class Task {
  public:
    Task(std::string_view name) noexcept;
    void end() noexcept;
    ~Task() {
        end();
    };
};
} // namespace itt

namespace log {

std::shared_ptr<spdlog::logger> get_or_nullsink(std::string const &);
} // namespace log
} // namespace dlstreamer
