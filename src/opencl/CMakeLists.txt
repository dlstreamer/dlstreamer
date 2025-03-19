# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

find_package(OpenCL)
if (OpenCL_FOUND)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/_plugin)

  add_subdirectory(opencl_tensor_normalize)

  find_package(PkgConfig)
  pkg_search_module(VA va libva)
  if(VA_FOUND)
    add_definitions(-DDLS_HAVE_VAAPI)
    add_subdirectory(vaapi_to_opencl)
  endif()

  add_subdirectory(_plugin/dlstreamer/opencl/elements)
  add_subdirectory(_gst_plugin)

endif()
