# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

find_package(OpenCV COMPONENTS core imgproc highgui)

if (OpenCV_FOUND)

    add_definitions(-DHAVE_OPENCV)

    add_subdirectory(tensor_normalize_opencv)

    set(TARGET_NAME "dlstreamer_opencv")

    add_library(${TARGET_NAME} SHARED register_elements.cpp)
    set_target_lib_version(${TARGET_NAME})
    set_compile_flags(${TARGET_NAME})

    target_link_libraries(${TARGET_NAME}
    PRIVATE
        tensor_normalize_opencv
        ${OpenCV_LIBS}
        dlstreamer_gst
        gstvideoanalyticsmeta
    )

    install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})

endif()
