# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "test_gstallocatorwrapper")

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTCHECK gstreamer-check-1.0 REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

project(${TARGET_NAME})

set(TEST_SOURCES
    allocator_mock/allocator_mock.h
    allocator_mock/gst_allocator_mock.h
    allocator_mock/gst_allocator_mock.cpp
    gst_allocator_wrapper_test.cpp
)

add_executable(${TARGET_NAME} ${TEST_SOURCES})

target_link_libraries(${TARGET_NAME}
PRIVATE
    gtest
    gmock
    common
    ${GSTREAMER_LIBRARIES}
    ${GSTCHECK_LIBRARIES}
    ${GLIB2_LIBRARIES}
)
target_include_directories(${TARGET_NAME}
PRIVATE
    ${GSTCHECK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/allocator_mock
    ${GLIB2_INCLUDE_DIRS}
)

add_test(NAME GST_ALLOCATOR_WRAPPER_TEST COMMAND ${TARGET_NAME})
