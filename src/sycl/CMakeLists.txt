# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

execute_process(COMMAND which dpcpp RESULT_VARIABLE status OUTPUT_VARIABLE out OUTPUT_QUIET )
if(status EQUAL 0)

    set(TARGET_NAME "dlstreamer_sycl")

    set(CMAKE_CXX_COMPILER dpcpp)
    set(CMAKE_CXX_STANDARD 17)
    string(REPLACE "-fno-strict-overflow" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

    add_subdirectory(tensor_histogram_sycl)
    add_subdirectory(watermark_sycl)
    add_subdirectory(_plugin/dlstreamer/sycl/elements)
    add_subdirectory(_gst_plugin)

else()
    message("Intel® oneAPI DPC++ Compiler not found, skipping ${TARGET_NAME} library")
endif()
