# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "gvapython")

pkg_check_modules(GLIB2 glib-2.0 REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.14 REQUIRED)
pkg_check_modules(PYGOBJECT pygobject-3.0)
find_package(PythonLibs 3.4 REQUIRED)

if (NOT PYGOBJECT_FOUND OR NOT PYTHONLIBS_FOUND)
    return()
endif()


file (GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    )

file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PRIVATE
    ${GLIB2_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${PYTHON_INCLUDE_DIRS}
    ${PYGOBJECT_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GLIB2_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${PYTHON_LIBRARIES}
    ${PYGOBJECT_LIBRARIES}
    common
)

install(TARGETS ${TARGET_NAME} DESTINATION lib/gst-video-analytics)
