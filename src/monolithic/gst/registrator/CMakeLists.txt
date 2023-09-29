# ==============================================================================
# Copyright (C) 2018 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "gstvideoanalytics")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)

file (GLOB MAIN_SRC
    register_elements.cpp
)

file (GLOB MAIN_HEADERS
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-variable -Wno-error=unused-parameter)
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    common
    elements
    inference_elements
    gvatrack
    gstdlstreamer_bins
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
