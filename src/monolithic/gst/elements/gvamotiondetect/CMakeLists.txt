# ==============================================================================
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.16)
include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0)
if(NOT MSVC)
  pkg_check_modules(GSTVA REQUIRED IMPORTED_TARGET gstreamer-va-1.0)
endif()

# Minimal source: plugin implementation only (no large inference converter bundle).
set(GVAMD_EXTRA_SOURCES ${CMAKE_SOURCE_DIR}/src/utils/utils.cpp ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/logger/logger.cpp ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/logger/perf_logger.cpp)

if(MSVC OR WIN32)
  set(GVAMD_MAIN_SOURCE gvamotiondetect_win.cpp)
else()
  set(GVAMD_MAIN_SOURCE gvamotiondetect.cpp)
endif()

add_library(gstgvamotiondetect MODULE ${GVAMD_MAIN_SOURCE} ${GVAMD_EXTRA_SOURCES})

if(MSVC)
  # Use generator expressions so MSVC multi-config builds place DLL at:
  #   <build>/intel64/<Config>/bin/<Config>/gstgvamotiondetect.dll
  # Import lib (.lib) at:
  #   <build>/intel64/<Config>/lib/<Config>/gstgvamotiondetect.lib
  set_target_properties(gstgvamotiondetect PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gstgvamotiondetect"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/intel64/$<CONFIG>/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/intel64/$<CONFIG>/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/intel64/$<CONFIG>/lib/$<CONFIG>"
  )
else()
  set_target_properties(gstgvamotiondetect PROPERTIES OUTPUT_NAME "gvamotiondetect" PREFIX "libgst")
endif()

# Disable ITT notify API to avoid linking against external instrumentation library
target_compile_definitions(gstgvamotiondetect PRIVATE INTEL_NO_ITTNOTIFY_API)

target_include_directories(gstgvamotiondetect PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${GST_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/include/dlstreamer/gst/videoanalytics/
  ${CMAKE_SOURCE_DIR}/src/utils/
  ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/include/
  ${CMAKE_SOURCE_DIR}/thirdparty/itt/include  # restore ITT header path for ENABLE_ITT builds
)

target_link_libraries(gstgvamotiondetect PRIVATE PkgConfig::GST dlstreamer_gst dlstreamer_gst_meta gstvideoanalyticsmeta)

if(NOT MSVC)
  # VAAPI / GstVA only for non-Windows builds
  target_link_libraries(gstgvamotiondetect PRIVATE
    PkgConfig::GSTVA
    dlstreamer_vaapi
  )
endif()

if(NOT MSVC)
  pkg_check_modules(LIBVA QUIET IMPORTED_TARGET libva)
  if (LIBVA_FOUND)
    target_link_libraries(gstgvamotiondetect PRIVATE PkgConfig::LIBVA)
  else()
    find_library(LIBVA_LIB va)
    if (LIBVA_LIB)
      target_link_libraries(gstgvamotiondetect PRIVATE ${LIBVA_LIB})
    endif()
  endif()
endif()

find_package(OpenCL QUIET)
if (OpenCL_FOUND)
  target_link_libraries(gstgvamotiondetect PRIVATE OpenCL::OpenCL)
  target_compile_definitions(gstgvamotiondetect PRIVATE HAVE_OPENCL_LOADER=1)
endif()

find_package(OpenCV QUIET COMPONENTS core imgproc)
if (OpenCV_FOUND)
  target_include_directories(gstgvamotiondetect PRIVATE ${OpenCV_INCLUDE_DIRS})
  target_link_libraries(gstgvamotiondetect PRIVATE ${OpenCV_LIBS})
  target_compile_definitions(gstgvamotiondetect PRIVATE HAVE_OPENCV=1)
endif()

install(TARGETS gstgvamotiondetect
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gstreamer-1.0
)
