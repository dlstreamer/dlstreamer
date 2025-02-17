# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

set (TARGET_NAME "ffmpeg_openvino_decode_inference")

## use C11/C++17
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 23)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV libavformat libavcodec libswscale libavutil REQUIRED)
find_package(OpenVINO COMPONENTS Runtime REQUIRED)
find_package(OpenCL REQUIRED)
find_package(gflags REQUIRED)

file (GLOB MAIN_SRC *.cpp)
file (GLOB MAIN_HEADERS *.h)

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})

target_include_directories(${TARGET_NAME} PRIVATE ${LIBAV_INCLUDE_DIRS})

target_link_directories(${TARGET_NAME} PRIVATE ${LIBAV_LIBRARY_DIRS})

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${LIBAV_LIBRARIES}
        openvino::runtime
        ${OpenCL_LIBRARY}
        gflags)

