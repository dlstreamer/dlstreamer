# ==============================================================================
# Copyright (C) 2020-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "test_preprocessing")

find_package(PkgConfig REQUIRED)

project(${TARGET_NAME})

file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(${TARGET_NAME} ${TEST_SOURCES})

target_link_libraries(${TARGET_NAME}
PRIVATE
    gtest
    gmock
    test_utils
    inference_elements
    image_inference_openvino
)

target_include_directories(${TARGET_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
