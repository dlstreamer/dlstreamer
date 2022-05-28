# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "gvatypes")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.18 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
    ${GLIB2_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    gvacommon
PRIVATE
    ${GLIB2_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    utils
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_LIBRARIES_INSTALL_PATH})
