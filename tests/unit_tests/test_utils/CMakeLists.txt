# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "test_utils")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTCHECK gstreamer-check-1.0 REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})

target_include_directories(${TARGET_NAME}
PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
  ${GSTCHECK_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
  ${GSTCHECK_LIBRARIES}
)
