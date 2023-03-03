/*******************************************************************************
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#include "config.h"
#include "dlstreamer_logger.h"

namespace dlstreamer::itt {

#ifdef ENABLE_ITT
#include <ittnotify.h>

__itt_domain *get_domain() {
    static __itt_domain *itt_domain = __itt_domain_create("video-analytics");
    return itt_domain;
}

Task::Task(std::string_view name) noexcept {
    auto itt_domain = get_domain();
    if (itt_domain)
        __itt_task_begin(itt_domain, __itt_null, __itt_null, __itt_string_handle_create(name.data()));
}

void Task::end() noexcept {
    auto itt_domain = get_domain();
    if (itt_domain)
        __itt_task_end(itt_domain);
}

#else

Task::Task(std::string_view name) noexcept {
}

void Task::end() noexcept {
}
#endif // ENABLE_ITT
} // namespace dlstreamer::itt