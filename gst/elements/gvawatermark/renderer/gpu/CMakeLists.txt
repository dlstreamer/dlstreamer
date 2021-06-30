# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_COMPILER dpcpp)
set(CMAKE_CXX_STANDARD 17)
string(REPLACE "-fno-strict-overflow" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

set (TARGET_NAME "gpurenderer")

find_package(OpenCV REQUIRED core gapi)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_search_module(VA va libva REQUIRED)

get_filename_component(CMAKE_PARENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PATH)

file (GLOB SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        )

file (GLOB HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_PARENT_SOURCE_DIR}/renderer.h
        )

add_library(${TARGET_NAME} SHARED ${SRC} ${HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
        ${VA_INCLUDE_DIRS}
        ${OPENCV_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_PARENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        ${OpenCV_LIBS}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        common
        ze_loader
        utils
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_LIBRARIES_INSTALL_PATH})
