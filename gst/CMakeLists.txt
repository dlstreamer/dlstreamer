# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

add_subdirectory(runtime_feature_toggling)
add_subdirectory(common)
add_subdirectory(elements)
add_subdirectory(inference_elements)
add_subdirectory(registrator)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    add_subdirectory(audio_inference_elements)
endif()

if (NOT (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64" OR ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm"))
    add_subdirectory(gvaitttracer)
endif()
