# ==============================================================================
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "preproc_base")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.18 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

if(UNIX AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0)
    # Function cast on definition of GvaPreprocBase gtype
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=cast-function-type)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
    ${GLIB2_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    # TODO: refactor mess in cmake targets.
    # Here we need model proc parser and pre proc
    inference_elements
    inference_backend
    json-schema-validator
    gvacommon
PRIVATE
    ${GLIB2_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    utils
    gvatypes
)
