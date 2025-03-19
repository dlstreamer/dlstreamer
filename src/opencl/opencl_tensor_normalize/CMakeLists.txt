# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME opencl_tensor_normalize)

find_package(OpenCL REQUIRED)

add_library(${TARGET_NAME} OBJECT opencl_tensor_normalize.cpp)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PRIVATE
        ${OpenCL_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        ${OpenCL_LIBRARY}
)
