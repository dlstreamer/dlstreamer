/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "dlstreamer_logger.h"

namespace dlstreamer::log {
std::shared_ptr<spdlog::logger> nullsink_instance() {
    static auto nullsink_logger = std::make_shared<spdlog::logger>("empty");
    return nullsink_logger;
}
std::shared_ptr<spdlog::logger> get_or_nullsink(const std::string &name) {
    auto logger = spdlog::get(name);
    return logger ? logger : nullsink_instance();
}
} // namespace dlstreamer::log
