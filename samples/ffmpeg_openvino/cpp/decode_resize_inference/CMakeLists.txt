# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

set (TARGET_NAME "ffmpeg_openvino_decode_resize_inference")

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

# use pkg-config if sample builds as standalone. Otherwise vars DLSTREAMER_INCLUDE_DIRS/etc set by top level cmake
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
    pkg_check_modules(DLSTREAMER dl-streamer REQUIRED)
else()
    target_include_directories(${TARGET_NAME}
            PRIVATE
            ${DLSTREAMER_BASE_DIR}/src/ffmpeg/_plugin
            ${DLSTREAMER_BASE_DIR}/src/vaapi/_plugin
            )
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
        ${DLSTREAMER_INCLUDE_DIRS}
        ${LIBAV_INCLUDE_DIRS}
)
target_link_directories(${TARGET_NAME} PRIVATE ${LIBAV_LIBRARY_DIRS})

target_link_libraries(${TARGET_NAME}
PRIVATE
        dlstreamer_ffmpeg
        dlstreamer_vaapi
        openvino::runtime
        ${LIBAV_LIBRARIES}
        ${OpenCL_LIBRARY}
        gflags)
