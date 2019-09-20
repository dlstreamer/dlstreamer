# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "gapi_pre_proc")

find_package(OpenCV COMPONENTS core imgproc imgcodecs gapi)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER gstreamer-1.0>=1.14 REQUIRED)

if(OpenCV_FOUND)
    add_definitions(-DHAVE_OPENCV)
endif()

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/gapi_pre_proc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/gapi_pre_proc.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/gapi_pre_proc.h
        )

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for released build type
# FIXME: hence it marked as error
target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-parameter)

target_include_directories(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_LIBRARIES}
        logger
        opencv_utils
PRIVATE
        ${OpenCV_LIBS}
)

install(TARGETS ${TARGET_NAME} DESTINATION lib/gst-video-analytics)
