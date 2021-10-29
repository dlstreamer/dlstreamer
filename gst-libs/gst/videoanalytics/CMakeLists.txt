# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set (TARGET_NAME "gstvideoanalyticsmeta")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.16 REQUIRED)
pkg_check_modules(GLIB2 glib-2.0 REQUIRED)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
        pkg_check_modules(GSTAUDIO gstreamer-audio-1.0>=1.16 REQUIRED)
endif()

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/metadata/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        ${CMAKE_CURRENT_SOURCE_DIR}/metadata/*.h
        )

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    file (GLOB AUDIO_META_SRC
         ${CMAKE_CURRENT_SOURCE_DIR}/audio/*.c
    )
    file (GLOB AUDIO_META_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/audio/*.h
    )
    list (APPEND MAIN_SRC ${AUDIO_META_SRC})
    list (APPEND MAIN_HEADERS ${AUDIO_META_HEADERS})
endif()

if(WIN32)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} ${MAIN_HEADERS})
set_target_lib_version(${TARGET_NAME})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_INCLUDE_DIRS}
        ${GLIB2_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/metadata
)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/audio
        ${GSTAUDIO_INCLUDE_DIRS}
    )
endif()

target_link_libraries(${TARGET_NAME}
PUBLIC
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${OpenCV_LIBS}
        ${GLIB2_LIBRARIES}
)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${GSTAUDIO_LIBRARIES})
endif()

configure_file(${CMAKE_SOURCE_DIR}/cmake/dl-streamer.pc.in ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pkgconfig/dl-streamer.pc @ONLY)

install(FILES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/pkgconfig/dl-streamer.pc DESTINATION ${DLSTREAMER_LIBRARIES_INSTALL_PATH}/pkgconfig/)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_LIBRARIES_INSTALL_PATH}/)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} DESTINATION ${DLSTREAMER_HEADERS_INSTALL_PATH}/gst FILES_MATCHING PATTERN "*.h")
