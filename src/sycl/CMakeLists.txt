# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

execute_process(COMMAND which icpx RESULT_VARIABLE status OUTPUT_VARIABLE out OUTPUT_QUIET )
if(status EQUAL 0)

    set(TARGET_NAME "dlstreamer_sycl")

    set(CMAKE_CXX_COMPILER icpx)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsycl -Wno-unused-command-line-argument")
    set(CMAKE_CXX_STANDARD 23)

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

    add_subdirectory(sycl_tensor_histogram)
    add_subdirectory(sycl_meta_overlay)
    add_subdirectory(_plugin/dlstreamer/sycl/elements)
    add_subdirectory(_gst_plugin)

else()
    message("Intel® oneAPI DPC++ Compiler not found, skipping ${TARGET_NAME} library")
endif()
