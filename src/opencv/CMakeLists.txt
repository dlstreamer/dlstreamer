# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

find_package(OpenCV COMPONENTS core imgproc)

if (OpenCV_FOUND)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

    add_subdirectory(opencv_find_contours)
    add_subdirectory(opencv_barcode_detector)
    add_subdirectory(opencv_object_association)
    add_subdirectory(opencv_tensor_normalize)
    add_subdirectory(opencv_cropscale)
    add_subdirectory(opencv_meta_overlay)
    add_subdirectory(opencv_remove_background)
    add_subdirectory(tensor_postproc_human_pose)
    find_package(OpenCL)
    if (OpenCL_FOUND AND OpenCV_VERSION VERSION_GREATER_EQUAL "4.4")
        add_definitions(-DDLS_HAVE_OPENCV_UMAT)
        add_subdirectory(opencv_warp_affine)
    endif()

    add_subdirectory(_plugin/dlstreamer/opencv/elements)
    add_subdirectory(_gst_plugin)
endif()
