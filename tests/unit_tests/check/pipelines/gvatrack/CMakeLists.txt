# ==============================================================================
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "test_gvatrack")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTCHECK gstreamer-check-1.0 REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})

target_include_directories(${TARGET_NAME}
PRIVATE
  ${DLSTREAMER_INCLUDE_DIRS}/../gst/elements/gvatrack/vas # !!
  ${DLSTREAMER_INCLUDE_DIRS}/../gst/elements/gvatrack # !!
  ${DLSTREAMER_BASE_DIR}/src/monolithic/gst/elements/gvatrack
  ${DLSTREAMER_INCLUDE_DIRS}/../gst/common
  ${DLSTREAMER_INCLUDE_DIRS}/../test
  ${GSTCHECK_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
  ${GSTCHECK_LIBRARIES}
  pipeline_test_common
  test_utils
  gtest
  pre_proc
  common
)

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME} WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
