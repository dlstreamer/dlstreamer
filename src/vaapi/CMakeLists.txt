# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

find_package(PkgConfig)
pkg_search_module(VA va libva)
if (VA_FOUND)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

    add_subdirectory(vaapi_sync)
    add_subdirectory(vaapi_batch_proc)

    add_subdirectory(_plugin/dlstreamer/vaapi/elements)
    add_subdirectory(_gst_plugin)
endif()
