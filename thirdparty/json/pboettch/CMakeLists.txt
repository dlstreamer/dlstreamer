# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.5)

option(BUILD_TESTS "" OFF)
option(BUILD_EXAMPLES "" OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "4.9.0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")
endif()

add_subdirectory(json-schema-validator-2.0.0)
