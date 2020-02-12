# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "image_inference")

add_subdirectory(openvino)

if(${HAVE_VAAPI})
        add_subdirectory(async_with_va_api)
endif()

find_package(PkgConfig REQUIRED)

find_package(InferenceEngine 2.1.0 REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/openvino
        ${CMAKE_CURRENT_SOURCE_DIR}/async_with_va_api
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        IE::inference_engine
        logger
)

install(TARGETS ${TARGET_NAME} DESTINATION lib/gst-video-analytics)
