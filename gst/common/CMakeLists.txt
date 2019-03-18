# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "common")

find_package(InferenceEngine 1.0 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)


file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

if (NOT ${DISABLE_VAAPI})
        pkg_search_module(VA va libva REQUIRED)
else()
        list(REMOVE_ITEM MAIN_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/gst_vaapi_plugins_utils.h)
endif()

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for released build type
# FIXME: hence it marked as error
target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-parameter)

target_include_directories(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        IE::inference_engine
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GLIB2_LIBRARIES}
        logger
        gstvideoanalyticsmeta
        jsonconvert
)

if (NOT ${DISABLE_VAAPI})
        target_include_directories(${TARGET_NAME} PUBLIC ${VA_INCLUDE_DIRS})

        target_link_libraries(${TARGET_NAME} PUBLIC ${VA_LIBRARIES})
endif()

install(TARGETS ${TARGET_NAME} DESTINATION lib/va-gstreamer-plugins)
