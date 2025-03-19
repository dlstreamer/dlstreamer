# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "vaapi_batch_proc")

find_package(PkgConfig)
pkg_search_module(VA va libva REQUIRED)


add_library(${TARGET_NAME} OBJECT vaapi_batch_proc.cpp)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${VA_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    dlstreamer_api
    ${VA_LIBRARIES}
)
