# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

if(WIN32)
    add_subdirectory(d3d11_wrapper)
    add_subdirectory(image_inference_async_d3d11)
endif()