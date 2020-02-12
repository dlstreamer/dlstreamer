# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "image_inference_openvino")

find_package(InferenceEngine 2.1.0 REQUIRED)

find_package(PkgConfig REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )
add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for release build type
# FIXME: hence it marked as error
target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-parameter)

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        IE::inference_engine
        logger
)

install(TARGETS ${TARGET_NAME} DESTINATION lib/gst-video-analytics)
