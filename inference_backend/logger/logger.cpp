/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/logger.h"
#include "stdio.h"
#include <string>

static GvaLogFuncPtr inference_log_function = nullptr;

void set_log_function(GvaLogFuncPtr log_func) {
    inference_log_function = log_func;
};

void debug_log(int level, const char *file, const char *function, int line, const char *message) {
    if (!inference_log_function) {
        set_log_function(default_log_function);
    }
    (*inference_log_function)(level, file, function, line, message);
};

void default_log_function(int level, const char *file, const char *function, int line, const char *message) {
    std::string log_level[] = {"DEFAULT", "ERROR", "WARNING", "FIXME", "INFO", "DEBUG", "LOG", "TRACE", "MEMDUMP"};
    fprintf(stderr, "%s \t %s:%i : %s \t %s \n", log_level[level].c_str(), file, line, function, message);
}
