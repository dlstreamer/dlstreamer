# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "gvamtcnn")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.16 REQUIRED)

file (GLOB MAIN_SRC
    register_gvamtcnn.c
    gvabboxregression/*.c
    gvabboxregression/*.cpp
    gvanms/*.c
    gvanms/*.cpp
    common/*.c
    common/*.cpp
)

file (GLOB MAIN_HEADERS
    gvabboxregression/*.h
    gvanms/*.h
    common/*.h
)

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-variable -Wno-error=unused-parameter)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    gvabboxregression
    gvanms
    common
    ${CMAKE_SOURCE_DIR}/../common/
    ${CMAKE_SOURCE_DIR}/src/utils/
    ${DLSTREAMER_BASE_DIR}/include/dlstreamer/gst/metadata/
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${OpenCV_LIBS}
    common
    utils
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
