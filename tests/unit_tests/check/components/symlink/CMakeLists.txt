# ==============================================================================
# Copyright (C) 2024-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "test_symlink")

find_package(PkgConfig REQUIRED)

project(${TARGET_NAME})

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/symlink.cpp
)

add_executable(${TARGET_NAME} ${TEST_SOURCES})

target_link_libraries(${TARGET_NAME}
PRIVATE
    gtest
    gmock
    common
    test_common
    test_utils
    inference_backend
    inference_elements
)

target_include_directories(${TARGET_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
