# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "logger")

add_library(${TARGET_NAME} STATIC logger.cpp perf_logger.cpp)

target_link_libraries(${TARGET_NAME} PUBLIC inference_backend)

if(${ENABLE_ITT})
    target_link_libraries(${TARGET_NAME} PUBLIC ittnotify)
endif()

