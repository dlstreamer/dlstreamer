# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "gstdlstreamer_bins")

cmake_minimum_required(VERSION 3.1)

add_subdirectory(model_proc)
add_subdirectory(splitjoinbin)
add_subdirectory(video_inference)
add_subdirectory(video_detect)
add_subdirectory(video_classify)
add_subdirectory(object_track)

file(GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        )

file(GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_gst
        splitjoinbin
        video_inference
        video_detect
        video_classify
        object_tracking
        )

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
