# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME opencv_meta_overlay)

find_package(OpenCV REQUIRED)

add_library(${TARGET_NAME} OBJECT opencv_meta_overlay.cpp)
set_compile_flags(${TARGET_NAME})
if(NOT(WIN32))
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-enum-enum-conversion")
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        base_meta_overlay
)
