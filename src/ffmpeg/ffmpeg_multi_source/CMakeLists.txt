# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

set(TARGET_NAME ffmpeg_multi_source)

find_package(PkgConfig)
pkg_check_modules(LIBAV libavformat libavcodec libswscale libavutil)

file(GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_library(${TARGET_NAME} OBJECT ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

set(DLSTREAMER_DIR $ENV{DLSTREAMER_DIR})

if(NOT DLSTREAMER_DIR)
    message(FATAL_ERROR "Please set the DLSTREAMER_DIR environment variable")
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
        ${LIBAV_INCLUDE_DIRS}
        ${DLSTREAMER_DIR}/include
)

target_link_directories(${TARGET_NAME} PUBLIC ${LIBAV_LIBRARY_DIRS})

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        dlstreamer_vaapi
        ${LIBAV_LIBRARIES}
)
