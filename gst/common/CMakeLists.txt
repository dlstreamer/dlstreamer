# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "common")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)


file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
        ${GSTALLOC_INCLUDE_DIRS}
        )

target_link_libraries(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GLIB2_LIBRARIES}
        logger
        gstvideoanalyticsmeta
        json-hpp
PRIVATE
        ${GSTALLOC_LIBRARIES}
)
if (${USE_VPUSMM})
        target_link_libraries(${TARGET_NAME} PUBLIC vpusmm)
endif()
