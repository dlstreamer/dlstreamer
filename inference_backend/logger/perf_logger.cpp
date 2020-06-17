/*******************************************************************************
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "inference_backend/logger.h"

#ifdef ENABLE_ITT

static __itt_domain *itt_domain = nullptr;

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
    if (itt_domain)
        __itt_task_begin(itt_domain, __itt_null, __itt_null, __itt_string_handle_create(name));
    else
        GVA_WARNING("ITTTask could not created.");
}

void ITTTask::taskEnd() {
    if (itt_domain)
        __itt_task_end(itt_domain);
}

#endif // ENABLE_ITT
