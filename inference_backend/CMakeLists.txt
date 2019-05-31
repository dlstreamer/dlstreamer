# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "inference_backend")

add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME}
INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/
)

add_subdirectory(image_inference/openvino)
add_subdirectory(pre_proc)
add_subdirectory(logger)
