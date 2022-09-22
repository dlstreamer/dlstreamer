# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.12)

find_package(OpenCV COMPONENTS core imgproc)

if (OpenCV_FOUND)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

    add_subdirectory(find_contours_opencv)
    add_subdirectory(object_association_opencv)
    add_subdirectory(tensor_normalize_opencv)
    add_subdirectory(video_cropscale_opencv)
    add_subdirectory(watermark_opencv)
    add_subdirectory(remove_background_opencv)
    find_package(OpenCL)
    if (OpenCL_FOUND AND OpenCV_VERSION VERSION_GREATER_EQUAL "4.4")
        add_definitions(-DDLS_HAVE_OPENCV_UMAT)
        add_subdirectory(warp_affine_opencv)
    endif()

    add_subdirectory(_plugin/dlstreamer/opencv/elements)
    add_subdirectory(_gst_plugin)
endif()
