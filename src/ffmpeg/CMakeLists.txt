# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.12)

find_package(PkgConfig)
pkg_search_module(LIBAV libavformat libavcodec libswscale libavutil)

if (LIBAV_FOUND)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

  add_subdirectory(multi_source_ffmpeg)

  add_subdirectory(_plugin/dlstreamer/ffmpeg/elements)
endif()
