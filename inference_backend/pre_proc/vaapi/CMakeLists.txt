# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "vaapi_pre_proc")

find_package(PkgConfig)
pkg_search_module(VA va libva REQUIRED)
pkg_search_module(VADRM va-drm libva-drm REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VA_INCLUDE_DIRS}
PRIVATE
    ${VADRM_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        ${VA_LIBRARIES}
        logger
PRIVATE
        ${VADRM_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION lib/va-gstreamer-plugins)
