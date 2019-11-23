# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "inference_elements")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)

file (GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaclassify/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaclassify/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvadetect/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvadetect/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvainference/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvainference/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/human_pose_estimation/*.cpp
)

file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaclassify/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gvadetect/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gvainference/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/human_pose_estimation/*.hpp
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for released build type
# FIXME: hence it marked as error
target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-variable -Wno-error=unused-parameter)

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/base/
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaclassify/
    ${CMAKE_CURRENT_SOURCE_DIR}/gvadetect/
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaskeleton/human_pose_estimation/
    ${CMAKE_CURRENT_SOURCE_DIR}/gvainference/
    PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${GSTALLOC_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${OpenCV_LIBS}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GSTALLOC_LIBRARIES}
    common
    image_inference
    image_inference_openvino
    pre_proc
    opencv_pre_proc
    gapi_pre_proc
    logger
    jsonconvert
)

if(${HAVE_VAAPI})
    target_link_libraries(${TARGET_NAME} PRIVATE va_api_wrapper image_inference_async)
endif()
