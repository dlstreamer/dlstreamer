# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "gvatensortometa")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTBASE gstreamer-base-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)
find_IE_package()

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/to_roi/blob_to_roi_converter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/to_roi/ov_default.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/to_tensor/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/common/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/to_roi/blob_to_roi_converter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/to_roi/ov_default.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters/to_tensor/*.hpp
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor
    ${CMAKE_CURRENT_SOURCE_DIR}/post_processor/converters
PRIVATE
    ${GLIB2_INCLUDE_DIRS}
    ${GSTBASE_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    inference_elements
    ${InferenceEngine_LIBRARIES}
PRIVATE
    json-schema-validator
    ${GLIB2_LIBRARIES}
    ${GSTBASE_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    inference_backend
    gvacommon
    utils
)
