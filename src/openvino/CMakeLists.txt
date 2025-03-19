# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

find_package(OpenVINO COMPONENTS Runtime)
if (OpenVINO_FOUND)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

    add_subdirectory(openvino_inference)

    add_subdirectory(_plugin/dlstreamer/openvino/elements)
    add_subdirectory(_gst_plugin)
endif() # OpenVINO_FOUND
