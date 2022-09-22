# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_cpu")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

add_subdirectory(rate_adjust)
add_subdirectory(tensor_convert)
add_subdirectory(tensor_histogram)
add_subdirectory(tensor_sliding_aggregate)
add_subdirectory(tensor_postproc)

add_subdirectory(_plugin/dlstreamer/cpu/elements)
add_subdirectory(_gst_plugin)
