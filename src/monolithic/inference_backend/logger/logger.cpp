/*******************************************************************************
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/logger.h"
#include "stdio.h"
#include <sstream>
#include <string>

void default_log_function(int level, const char *file, const char *function, int line, const char *format,
                          va_list args) {
    static std::string log_level[] = {"DEFAULT", "ERROR", "WARNING", "FIXME",  "INFO",
                                      "DEBUG",   "LOG",   "TRACE",   "MEMDUMP"};

    std::stringstream fmt;
    fmt << log_level[level] << "\t" << file << ':' << line << ':' << function << ": " << format << "\n";
    vfprintf(stderr, fmt.str().c_str(), args);
}

static GvaLogFuncPtr inference_log_function = default_log_function;

void set_log_function(GvaLogFuncPtr log_func) {
    if (inference_log_function != log_func)
        inference_log_function = log_func;
};

void debug_log(int level, const char *file, const char *function, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    (*inference_log_function)(level, file, function, line, format, args);
    va_end(args);
};
