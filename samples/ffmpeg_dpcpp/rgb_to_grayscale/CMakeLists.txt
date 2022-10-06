# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "ffmpeg_dpcpp_rgb_to_grayscale")

cmake_minimum_required(VERSION 3.1)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV libavformat libavcodec libswscale libavutil)
pkg_check_modules(LIBVA libva libva-drm)
pkg_check_modules(LEVEL_ZERO libze_loader)
find_package(gflags)
execute_process(COMMAND which dpcpp RESULT_VARIABLE dpcpp_status OUTPUT_VARIABLE out OUTPUT_QUIET)

if (dpcpp_status EQUAL 0 AND LIBAV_FOUND AND LIBVA_FOUND AND LEVEL_ZERO_FOUND AND gflags_FOUND)

set(CMAKE_CXX_COMPILER dpcpp)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-debug-disables-optimization")
string(REPLACE "-fno-strict-overflow" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#set(CMAKE_CXX_STANDARD 17)

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
)

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${LIBAV_LIBRARIES}
        ${LEVEL_ZERO_LIBRARIES}
        dlstreamer_ffmpeg
        dlstreamer_vaapi
        gflags
)

else()

if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
    message(FATAL_ERROR "dpcpp compiler not found")
endif()

endif()
