/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "logger_functions.h"

#include <gst/gst.h>
GST_DEBUG_CATEGORY_STATIC(GVA_common);
#define GST_CAT_DEFAULT GVA_common

void GST_logger(int level, const char *file, const char *function, int line, const char *format, va_list args) {
    static bool is_initialized = false;
    if (!is_initialized) {
        GST_DEBUG_CATEGORY_INIT(GVA_common, "GVA_common", 0, "debug category for GVA common");
        is_initialized = true;
    }
// Workaround for GCC poison mark
#ifndef GST_DISABLE_GST_DEBUG
    gst_debug_log_valist(GVA_common, static_cast<GstDebugLevel>(level), file, function, line, NULL, format, args);
#endif // GST_DISABLE_GST_DEBUG
}
