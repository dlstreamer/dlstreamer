# ==============================================================================
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "test_inference_utils")

project(${TARGET_NAME})

set(TEST_SOURCES
    main_test.cpp
    model_proc_size_check.cpp
)

add_executable(${TARGET_NAME} ${TEST_SOURCES})

target_link_libraries(${TARGET_NAME}
PRIVATE
    gtest
PUBLIC
    utils
)
target_include_directories(${TARGET_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME})
