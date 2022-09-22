# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "dlstreamer_gst_sycl")

set(CMAKE_CXX_COMPILER dpcpp)
set(CMAKE_CXX_STANDARD 17)
string(REPLACE "-fno-strict-overflow" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/../*.cpp
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/../*.h
        )

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
PRIVATE
        ${GSTALLOC_INCLUDE_DIRS}
        )

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        dlstreamer_gst_utils
        dlstreamer_gst_meta
PRIVATE
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GLIB2_LIBRARIES}
        ${GSTALLOC_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
