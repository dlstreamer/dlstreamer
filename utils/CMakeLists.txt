# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "utils")

file (GLOB MAIN_SRC utils.cpp so_loader.h)
file (GLOB MAIN_HEADERS utils.h so_loader.cpp)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME} 
        ${CMAKE_DL_LIBS} 
        logger
)
