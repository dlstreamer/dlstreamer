# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

find_package(OpenCL)
if (OpenCL_FOUND)

if(${ENABLE_VAAPI})
add_subdirectory(video_preproc_vaapi_opencl)
endif()

add_subdirectory(tensor_normalize_opencl)

set(TARGET_NAME "dlstreamer_opencl")

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
    tensor_normalize_opencl
    dlstreamer_gst
    gstvideoanalyticsmeta
)

if(${ENABLE_VAAPI})
target_link_libraries(${TARGET_NAME} PUBLIC preproc_vaapi_opencl)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})

endif()
