# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "gvaitttracer")

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# TODO: Remove once upgraded to gst-1.18 or higher
add_definitions(-DGST_USE_UNSTABLE_API)

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_SOURCE_DIR}/src/utils/
PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
)

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${GSTREAMER_LIBRARIES}
        ${GLIB2_LIBRARIES}
        ittnotify
        utils
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
