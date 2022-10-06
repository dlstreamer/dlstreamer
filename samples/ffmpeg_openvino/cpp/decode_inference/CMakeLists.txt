# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "ffmpeg_openvino_decode_inference")

cmake_minimum_required(VERSION 3.1)

## use C11/C++17
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV libavformat libavcodec libswscale libavutil)
find_package(OpenVINO COMPONENTS Runtime)
find_package(OpenCL)
find_package(gflags)

if (LIBAV_FOUND AND OpenVINO_FOUND AND OpenCL_FOUND AND gflags_FOUND)

file (GLOB MAIN_SRC *.cpp)
file (GLOB MAIN_HEADERS *.h)

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${LIBAV_LIBRARIES}
        openvino::runtime
        ${OpenCL_LIBRARY}
        gflags)

endif()
