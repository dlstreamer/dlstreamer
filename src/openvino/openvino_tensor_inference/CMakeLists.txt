# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "openvino_tensor_inference")

find_package(OpenVINO REQUIRED Runtime)

add_library(${TARGET_NAME} OBJECT openvino_tensor_inference.cpp)
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PRIVATE
    openvino::runtime
    dlstreamer_api
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
