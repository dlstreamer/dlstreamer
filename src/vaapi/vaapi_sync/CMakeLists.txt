# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "vaapi_sync")

find_package(PkgConfig)
pkg_search_module(VA va libva REQUIRED)

file(GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_library(${TARGET_NAME} OBJECT ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${VA_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    dlstreamer_api
    ${VA_LIBRARIES}
)
