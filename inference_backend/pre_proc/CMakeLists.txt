# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(opencv_utils)
add_subdirectory(opencv)

set (TARGET_NAME "pre_proc")

file (GLOB MAIN_SRC *.cpp)

file (GLOB MAIN_HEADERS *.h)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})

target_link_libraries(${TARGET_NAME}
PRIVATE
        inference_backend
        opencv_pre_proc
        utils
)

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
