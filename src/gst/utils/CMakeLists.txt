# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "dlstreamer_gst_utils")

add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME}
INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )
