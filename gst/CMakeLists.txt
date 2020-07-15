# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

add_subdirectory(runtime_feature_toggling)
add_subdirectory(common)
add_subdirectory(elements)
add_subdirectory(inference_elements)
add_subdirectory(registrator)

if (NOT (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm"))
    add_subdirectory(gvaitttracer)
endif()
