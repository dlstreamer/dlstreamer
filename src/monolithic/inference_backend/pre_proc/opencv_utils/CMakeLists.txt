# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "opencv_utils")

find_package(OpenCV COMPONENTS core imgproc imgcodecs)
if(OpenCV_FOUND)
    add_definitions(-DHAVE_OPENCV)
endif()

find_package(PkgConfig REQUIRED)

file (GLOB MAIN_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/*.c
        )

file (GLOB MAIN_HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/*.h
        )

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: there are some debug information that are removed for released build type
# FIXME: hence it marked as error
if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=unused-parameter)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
        logger
PRIVATE
        ${OpenCV_LIBS}
)
