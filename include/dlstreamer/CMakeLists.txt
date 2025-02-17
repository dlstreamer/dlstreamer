# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

set (TARGET_NAME "dlstreamer_api")

add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME}
INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        )

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION ${DLSTREAMER_HEADERS_INSTALL_PATH} FILES_MATCHING PATTERN "*.h")