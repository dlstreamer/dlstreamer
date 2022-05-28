# ==============================================================================
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "gvatrack")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.18 REQUIRED)

file (GLOB_RECURSE MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR} *.c *.cpp
)
file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-variable -Wno-error=unused-parameter -Wno-error=reorder)
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
    ${GLIB2_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vas/modules/api/ot/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/vas/modules/common/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/vas/modules/platform/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/vas/modules/kernels/kalman_filter/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/vas/modules/components/ot/inc
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GLIB2_LIBRARIES}
    common
    utils
    ${ADDITIONAL_LINKS}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
