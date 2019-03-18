# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "face_attributes")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)


file (GLOB MAIN_SRC *.cpp)

file (GLOB MAIN_HEADERS *.h)

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

set_target_properties(${TARGET_NAME} PROPERTIES CMAKE_CXX_STANDARD 14)

target_include_directories(${TARGET_NAME}
PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
        include
)

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${OpenCV_LIBS}
        ${GLIB2_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        gstvideoanalyticsmeta
        common
        stdc++fs
    )
