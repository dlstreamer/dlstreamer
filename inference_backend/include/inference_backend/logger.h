/*******************************************************************************
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once

#include "config.h"
#include <stdarg.h>

enum {
    GVA_ERROR_LOG_LEVEL = 1,
    GVA_WARNING_LOG_LEVEL,
    GVA_FIXME_LOG_LEVEL,
    GVA_INFO_LOG_LEVEL,
    GVA_DEBUG_LEVEL,
    GVA_LOG_LOG_LEVEL,
    GVA_TRACE_LOG_LEVEL,
    GVA_MEMDUMP_LOG_LEVEL,
};

using GvaLogFuncPtr = void (*)(int level, const char *file, const char *function, int line, const char *format,
                               va_list args);

void set_log_function(GvaLogFuncPtr log_func);
void debug_log(int level, const char *file, const char *function, int line, const char *format, ...)
#ifdef __linux__
    __attribute__((__format__(__printf__, 5, 6)))
#endif
    ;

#define GVA_DEBUG_LOG(level, format, ...)                                                                              \
    do {                                                                                                               \
        debug_log(level, __FILE__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__);                                     \
    } while (0)

#define GVA_MEMDUMP(format, ...) GVA_DEBUG_LOG(GVA_MEMDUMP_LOG_LEVEL, format, ##__VA_ARGS__)
#define GVA_TRACE(format, ...) GVA_DEBUG_LOG(GVA_TRACE_LOG_LEVEL, format, ##__VA_ARGS__)
#define GVA_LOG(format, ...) GVA_DEBUG_LOG(GVA_LOG_LOG_LEVEL, format, ##__VA_ARGS__)
#define GVA_DEBUG(format, ...) GVA_DEBUG_LOG(GVA_DEBUG_LEVEL, format, ##__VA_ARGS__)
#define GVA_INFO(format, ...) GVA_DEBUG_LOG(GVA_INFO_LOG_LEVEL, format, ##__VA_ARGS__)
#define GVA_FIXME(format, ...) GVA_DEBUG_LOG(GVA_FIXME_LOG_LEVEL, format, ##__VA_ARGS__)
#define GVA_WARNING(format, ...) GVA_DEBUG_LOG(GVA_WARNING_LOG_LEVEL, format, ##__VA_ARGS__)
#define GVA_ERROR(format, ...) GVA_DEBUG_LOG(GVA_ERROR_LOG_LEVEL, format, ##__VA_ARGS__)

#if defined(ENABLE_ITT) && defined(__cplusplus)
#include "ittnotify.h"
#include <string>

#define ITT_TASK(NAME) ITTTask task(NAME)

class ITTTask {
  public:
    ITTTask(const char *name);
    ITTTask(const std::string &name);
    ~ITTTask();

  private:
    void taskBegin(const char *name);
    void taskEnd();
};

#else

#define ITT_TASK(NAME)

#endif
