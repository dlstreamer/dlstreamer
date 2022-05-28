# ==============================================================================
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "gstvideoanalytics_preview")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.18 REQUIRED)

file(GLOB MAIN_SRC
    register_elements.cpp
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_compile_options(${TARGET_NAME} PRIVATE)

target_include_directories(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    gvaactionrecognitionbin
    gvainferencebin
    gvadetectbin
    gvaclassifybin
    gvafilter
    gvahistory
    gvatensoracc
    gvatensorconverter
    gvatensortometa
    gvavideototensor
    gvadrop
    gvatypes
    gvacommon
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
