# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

add_library(dlstreamer_logger STATIC dlstreamer_logger.cpp dlstreamer_itt.cpp dlstreamer_logger.h)

target_include_directories(dlstreamer_logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dlstreamer_logger PUBLIC spdlog::spdlog)
if(${ENABLE_ITT})
    target_link_libraries(dlstreamer_logger PUBLIC ittnotify)
endif()