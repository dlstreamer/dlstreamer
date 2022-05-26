# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

find_package(OpenVINO COMPONENTS Runtime)

# OV 2.0
# CI build system has old OV 2.0 build, so disable element on Windows until OV version is updated
if(OpenVINO_FOUND AND NOT(WIN32))

set(TARGET_NAME "dlstreamer_openvino_2_0_api")

add_definitions(-DHAVE_OPENVINO2)

find_package(PkgConfig REQUIRED)

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${DLSTREAMER_BASE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    openvino::runtime
    dlstreamer_gst
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})

endif() # OpenVINO_FOUND
