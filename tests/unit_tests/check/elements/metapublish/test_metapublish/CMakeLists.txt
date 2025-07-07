# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "test_metapublish_plugin")


file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_executable(${TARGET_NAME} ${MAIN_SRC} ${MAIN_HEADERS})
target_link_libraries(${TARGET_NAME}
PRIVATE
        gvametapublish
        gstvideoanalyticsmeta
        test_common
        test_utils
	fff
)

if(${ENABLE_PAHO_INSTALLATION})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPAHO_INC ")
        find_library(UUID uuid REQUIRED)
        target_link_libraries(${TARGET_NAME}
        PRIVATE
                uuid
        )
        # Need this for fff to work properly
        target_link_options(${TARGET_NAME} PRIVATE -rdynamic)
endif()

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/metapublish_test_files)

add_test(NAME ${TARGET_NAME} COMMAND ${TARGET_NAME} WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
