# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(meta_aggregate)
add_subdirectory(rate_adjust)
add_subdirectory(tensor_convert)
add_subdirectory(tensor_split_batch)
add_subdirectory(video_roi_split)

set(TARGET_NAME "dlstreamer_elements") # TODO better name?

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PRIVATE
    meta_aggregate
    rate_adjust
    tensor_convert
    tensor_split_batch
    video_roi_split
    dlstreamer_gst
    gstvideoanalyticsmeta
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
