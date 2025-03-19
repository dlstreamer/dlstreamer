# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME opencv_cropscale)

find_package(OpenCV REQUIRED)

add_library(${TARGET_NAME} OBJECT opencv_cropscale.cpp)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PRIVATE
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        dlstreamer_logger
        ${OpenCV_LIBS}
)
