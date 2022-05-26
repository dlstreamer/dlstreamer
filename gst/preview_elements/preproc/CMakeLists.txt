# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(base)
add_subdirectory(opencv)

if (${ENABLE_VAAPI})
    add_subdirectory(vaapi)
endif()
