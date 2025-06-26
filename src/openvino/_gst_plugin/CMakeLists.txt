# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "gstdlstreamer_openvino")

find_package(OpenVINO REQUIRED)

add_library(${TARGET_NAME} SHARED register_elements.cpp  ../_plugin/dlstreamer/openvino/elements/elements.cpp)
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PRIVATE
    openvino::runtime
    logger
    dlstreamer_openvino
    dlstreamer_gst
    openvino_inference
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
