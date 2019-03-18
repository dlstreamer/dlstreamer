# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "gstvideoanalytics")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)

file (GLOB MAIN_SRC
        gvawatermark/*.cpp
        gvawatermark/*.c
        gvaclassify/*.cpp
        gvaidentify/*.c
        gvaidentify/*.cpp
        gvaclassify/*.c
        gvainference/*.cpp
        gvainference/*.c
        gvametaconvert/*.cpp
        gvametaconvert/*.c
        elements.c
        )

file (GLOB MAIN_HEADERS
        gvawatermark/*.h
        gvaclassify/*.h
        gvaidentify/*.h
        gvainference/*.h
        gvametaconvert/*.h
        )

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for released build type
# FIXME: hence it marked as error
target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-variable -Wno-error=unused-parameter)

target_include_directories(${TARGET_NAME}
PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GSTALLOC_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        gvawatermark
        gvaclassify
        gvaidentify
        gvainference
        gvametaconvert
)

target_link_libraries(${TARGET_NAME}
PRIVATE
        ie_cpu_extension
        ${OpenCV_LIBS}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GSTALLOC_LIBRARIES}
        common
        image_inference_openvino
        pre_proc
        opencv_pre_proc
        logger
        jsonconvert
)

if(NOT ${DISABLE_VAAPI})
target_link_libraries(${TARGET_NAME} PRIVATE vaapi_pre_proc)
endif()


install(TARGETS ${TARGET_NAME} DESTINATION lib/va-gstreamer-plugins)
