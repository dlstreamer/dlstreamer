# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "logger")

file (GLOB MAIN_SRC *.cpp)

file (GLOB MAIN_HEADERS *.h)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})

target_link_libraries(${TARGET_NAME} inference_backend)

if(${ENABLE_ITT})
    target_link_libraries(${TARGET_NAME} ittnotify)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
