# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "gvacommon")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.16 REQUIRED)
find_IE_package()

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capabilities/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/meta/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/capabilities/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/meta/*.hpp
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})
if(UNIX)
    # TODO: fix compilation on CentOS
    target_compile_options(${TARGET_NAME} PUBLIC -Wno-error=missing-field-initializers)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GSTVIDEO_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    ${InferenceEngine_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    inference_backend
)

if(${ENABLE_VAAPI})
    target_link_libraries(${TARGET_NAME} PRIVATE va_api_wrapper)
endif()
