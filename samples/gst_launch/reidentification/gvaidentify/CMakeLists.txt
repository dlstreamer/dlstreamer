# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "gstgvaidentify")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.14 REQUIRED)
find_package(OpenCV REQUIRED core imgproc)

file (GLOB MAIN_SRC
    *.c
    *.cpp
)

file (GLOB MAIN_HEADERS
    *.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-variable -Wno-error=unused-parameter)

target_include_directories(${TARGET_NAME}
PRIVATE
    ${DLSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    common
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${OpenCV_LIBS}
    json-hpp
    json-schema-validator
    common
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
