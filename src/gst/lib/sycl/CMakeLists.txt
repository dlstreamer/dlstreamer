# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "dlstreamer_gst_sycl")

set(CMAKE_CXX_COMPILER icpx)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsycl -Wno-unused-command-line-argument")
set(CMAKE_CXX_STANDARD 23)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/../*.cpp
        )

add_library(${TARGET_NAME} SHARED ${MAIN_SRC})
set_compile_flags(${TARGET_NAME})

execute_process(COMMAND icpx --version OUTPUT_VARIABLE ICPX_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCH "[0-9]+\\.[0-9]+" ICPX_MAJOR_VERSION "${ICPX_VERSION}")
if(ICPX_MAJOR_VERSION VERSION_GREATER_EQUAL "2023.0")
        target_compile_options(${TARGET_NAME} PRIVATE -Wno-enum-constexpr-conversion)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
PRIVATE
        ${GSTALLOC_INCLUDE_DIRS}
        )

target_link_libraries(${TARGET_NAME}
PUBLIC
        dlstreamer_api
        dlstreamer_gst_utils
        dlstreamer_gst_meta
PRIVATE
        gst_logger_sink
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GLIB2_LIBRARIES}
        ${GSTALLOC_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
