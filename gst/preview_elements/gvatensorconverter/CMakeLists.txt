# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "gvatensorconverter")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTBASE gstreamer-base-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB HDR_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)

add_library(${TARGET_NAME} STATIC ${SRC_FILES} ${HDR_FILES})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
    ${GSTBASE_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GLIB2_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    ${GSTBASE_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GLIB2_LIBRARIES}
    gvacommon
)

if(${ENABLE_VAAPI})
    target_link_libraries(${TARGET_NAME} PRIVATE va_api_wrapper)
endif()
