/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG

#include <gst/gstinfo.h>
#include <spdlog/spdlog.h>
#include <string>

namespace dlstreamer::log {
std::shared_ptr<spdlog::logger> init_logger(GstDebugCategory *category, GObject *object);
std::string get_logger_name(GstDebugCategory *category, GObject *object);
} // namespace dlstreamer::log
