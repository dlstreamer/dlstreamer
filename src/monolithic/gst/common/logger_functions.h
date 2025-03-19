/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include <stdarg.h>

void GST_logger(int level, const char *file, const char *function, int line, const char *format, va_list args);

// FWD
struct _GstDebugCategory;

_GstDebugCategory *get_gva_debug_category();