/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "logger_functions.h"

#include <gst/gst.h>

static GstDebugCategory *init_gva_debug_cat() {
    static GstDebugCategory *gva_common = nullptr;
    GST_DEBUG_CATEGORY_INIT(gva_common, "GVA_common", 0, "debug category for GVA common");
    return gva_common;
}

void GST_logger(int level, const char *file, const char *function, int line, const char *format, va_list args) {
    static auto *gva_debug = init_gva_debug_cat();

// Workaround for GCC poison mark
#ifndef GST_DISABLE_GST_DEBUG
    gst_debug_log_valist(gva_debug, static_cast<GstDebugLevel>(level), file, function, line, NULL, format, args);
#endif // GST_DISABLE_GST_DEBUG
}
