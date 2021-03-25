# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "gstaudioanalytics")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)

file (GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/openvino/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/base/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/openvino/*.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

# FIXME: implicit declaration of tracer functions
if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=implicit-function-declaration)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/utils/
    ${CMAKE_SOURCE_DIR}/inference_backend/include/inference_backend/
    gvaaudiodetect
    base
    openvino
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTALLOC_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    utils
PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTALLOC_LIBRARIES}
    common
    gstvideoanalyticsmeta
    inference_backend
    inference_elements
    ${InferenceEngine_LIBRARIES}
    image_inference_openvino
    logger
    pre_proc
    opencv_pre_proc
    json-schema-validator
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
