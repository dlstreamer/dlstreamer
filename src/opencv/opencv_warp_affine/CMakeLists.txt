# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME opencv_warp_affine)

find_package(OpenCV REQUIRED)
find_package(OpenCL REQUIRED)

add_library(${TARGET_NAME} OBJECT opencv_warp_affine.cpp)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        ${OpenCV_LIBS}
        ${OpenCL_LIBRARY}
)
