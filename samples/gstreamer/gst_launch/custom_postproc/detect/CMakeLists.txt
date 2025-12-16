# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.10)
project(custom_postproc_libraries)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_ANALYTICS REQUIRED gstreamer-analytics-1.0)

link_directories(${TARGET_NAME} /opt/intel/dlstreamer/lib
        /opt/intel/dlstreamer/gstreamer/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib64
)

add_library(custom_postproc_detect SHARED
    custom_postproc_detect.cpp
)

target_include_directories(custom_postproc_detect PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_ANALYTICS_INCLUDE_DIRS}
)

target_link_libraries(custom_postproc_detect
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_ANALYTICS_LIBRARIES}
)

target_compile_options(custom_postproc_detect PRIVATE
    ${GSTREAMER_CFLAGS_OTHER}
    ${GSTREAMER_ANALYTICS_CFLAGS_OTHER}
)

set_target_properties(custom_postproc_detect PROPERTIES
    OUTPUT_NAME "custom_postproc_detect"
    PREFIX "lib"
    SUFFIX ".so"
)

target_link_options(custom_postproc_detect PRIVATE
    "-Wl,--no-undefined"
)
