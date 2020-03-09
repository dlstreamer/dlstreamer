/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#pragma once
#include "config.h"
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

#define GVA_DEBUG_LOG(level, message) debug_log(level, __FILE__, __FUNCTION__, __LINE__, message);

#define GVA_MEMDUMP(message) GVA_DEBUG_LOG(GVA_MEMDUMP_LOG_LEVEL, message);
#define GVA_TRACE(message) GVA_DEBUG_LOG(GVA_TRACE_LOG_LEVEL, message);
#define GVA_LOG(message) GVA_DEBUG_LOG(GVA_LOG_LOG_LEVEL, message);
#define GVA_DEBUG(message) GVA_DEBUG_LOG(GVA_DEBUG_LEVEL, message);
#define GVA_INFO(message) GVA_DEBUG_LOG(GVA_INFO_LOG_LEVEL, message);
#define GVA_FIXME(message) GVA_DEBUG_LOG(GVA_FIXME_LOG_LEVEL, message);
#define GVA_WARNING(message) GVA_DEBUG_LOG(GVA_WARNING_LOG_LEVEL, message);
#define GVA_ERROR(message) GVA_DEBUG_LOG(GVA_ERROR_LOG_LEVEL, message);

using GvaLogFuncPtr = void (*)(int level, const char *file, const char *function, int line, const char *message);

void set_log_function(GvaLogFuncPtr log_func);

void debug_log(int level, const char *file, const char *function, int line, const char *message);

void default_log_function(int level, const char *file, const char *function, int line, const char *message);

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
