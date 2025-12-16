# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "test_audio_detection_plugin")
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTAUDIO gstreamer-audio-1.0>=1.16 REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})

target_link_libraries(${TARGET_NAME}
PRIVATE
        ${GSTAUDIO_LIBRARIES}
        gstvideoanalyticsmeta
        common
        test_common
        test_utils
)

set(AUIDO_TEST_FILE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/audio_test_files")

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/test_files ${AUIDO_TEST_FILE_DIRECTORY})


add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME} WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
