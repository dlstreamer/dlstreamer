/*******************************************************************************
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/logger.h"

#ifdef HAVE_ITT

ITTTask::ITTTask(const char *name) {
    taskBegin(name);
}

ITTTask::ITTTask(const std::string &name) {
    taskBegin(name.c_str());
}

ITTTask::~ITTTask() {
    taskEnd();
}

void ITTTask::taskBegin(const char *name) {
    if (itt_domain == nullptr) {
        itt_domain = __itt_domain_create("video-analytics");
    }
    __itt_task_begin(itt_domain, __itt_null, __itt_null, __itt_string_handle_create(name));
}

void ITTTask::taskEnd() {
    __itt_task_end(itt_domain);
}

#endif // HAVE_ITT
