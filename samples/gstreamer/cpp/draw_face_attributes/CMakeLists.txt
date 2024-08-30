# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.12)

set (TARGET_NAME "draw_face_attributes")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTANALYTICS gstreamer-analytics-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

file (GLOB MAIN_SRC *.cpp)

file (GLOB MAIN_HEADERS *.h)

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})

set_target_properties(${TARGET_NAME} PROPERTIES CMAKE_CXX_STANDARD 14)

# use pkg-config if sample builds as standalone. Otherwise vars DLSTREAMER_INCLUDE_DIRS/etc set by top level cmake
if (${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${PROJECT_SOURCE_DIR})
    pkg_check_modules(DLSTREAMER dl-streamer REQUIRED)
else()
    target_include_directories(${TARGET_NAME}
    PRIVATE
        ${DLSTREAMER_BASE_DIR}/include/dlstreamer/gst
        ${DLSTREAMER_BASE_DIR}/include/
    )
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
        ${DLSTREAMER_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${OpenCV_LIBS}
        ${GLIB2_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GSTANALYTICS_LIBRARIES}
    )
