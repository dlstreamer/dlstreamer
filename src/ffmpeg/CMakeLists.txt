# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

find_package(PkgConfig)
pkg_search_module(LIBAV libavformat libavcodec libswscale libavutil)

if (LIBAV_FOUND)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

  add_subdirectory(ffmpeg_multi_source)

  add_subdirectory(_plugin/dlstreamer/ffmpeg/elements)
endif()
