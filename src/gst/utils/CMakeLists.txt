# ==============================================================================
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "dlstreamer_gst_utils")

add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME}
INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
        )
