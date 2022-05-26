# ==============================================================================
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

add_subdirectory(gst)
add_subdirectory(opencl)
add_subdirectory(opencv)
add_subdirectory(openvino)
add_subdirectory(postproc)

if(${ENABLE_VAAPI})
add_subdirectory(vaapi)
endif()
