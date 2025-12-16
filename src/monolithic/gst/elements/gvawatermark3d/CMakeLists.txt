# ==============================================================================
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "gvawatermark3d")

pkg_search_module(VA va libva REQUIRED)
find_package(OpenCV REQUIRED core imgproc calib3d)

file(GLOB WATERMARK3D_SRC
    *.cpp
    *.c
)

file(GLOB WATERMARK3D_HEADERS
    *.h
)

add_library(gvawatermark3d SHARED gvawatermark3d.cpp gvawatermark3d.h)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GSTALLOC_INCLUDE_DIRS}
)

target_link_directories(${TARGET_NAME} PUBLIC ${VA_LIBRARY_DIRS})
target_link_libraries(${TARGET_NAME}
    PRIVATE
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GSTALLOC_LIBRARIES}
        ${OpenCV_LIBS}
        dlstreamer_api
        common
        image_inference
        image_inference_openvino
        logger
        json-hpp
        json-schema-validator
        utils
)
