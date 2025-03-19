# ==============================================================================
# Copyright (C) 2022-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "openvino_inference_backend")

find_package(OpenVINO REQUIRED Runtime)

add_library(${TARGET_NAME} OBJECT openvino.cpp openvino.hpp)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME}
PUBLIC
    dlstreamer_api
PRIVATE
    openvino::runtime
    dlstreamer_logger
)
