# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "opencv_pre_proc")

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
        opencv_utils
PRIVATE
        ${OpenCV_LIBS}
)

if(WIN32)
    find_package(dlfcn-win32 REQUIRED)
    target_include_directories(${TARGET_NAME} PRIVATE ${dlfcn-win32_INCLUDE_DIRS})
    target_link_libraries(${TARGET_NAME} PRIVATE ${dlfcn-win32_LIBRARIES})
endif()
