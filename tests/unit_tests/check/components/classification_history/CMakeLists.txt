# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "test_classification_history")

find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED core imgproc imgcodecs)

pkg_check_modules(GSTCHECK gstreamer-check-1.0 REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

project(${TARGET_NAME})

set(TEST_SOURCES
    classification_history_tests.cpp
    main_test.cpp
)

add_executable(${TARGET_NAME} ${TEST_SOURCES})

target_link_libraries(${TARGET_NAME}
PRIVATE
    gtest
    gmock
    common
    inference_elements
    inference_backend
    test_common
    test_utils
    gstvideoanalyticsmeta
    ${GSTREAMER_LIBRARIES}
    ${GSTCHECK_LIBRARIES}
    ${GLIB2_LIBRARIES}
)
target_include_directories(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTCHECK_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GLIB2_INCLUDE_DIRS}
)

add_test(NAME GST_SKIP_CLASSIFICATION_TEST COMMAND ${TARGET_NAME})
