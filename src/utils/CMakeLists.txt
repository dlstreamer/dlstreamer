# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "utils")

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
        )

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${TARGET_NAME}
        ${CMAKE_DL_LIBS}
        logger
)
