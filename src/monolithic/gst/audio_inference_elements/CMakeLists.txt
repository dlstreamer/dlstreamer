# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (TARGET_NAME "audio_inference_elements")

find_package(PkgConfig REQUIRED)
find_package(OpenVINO REQUIRED Runtime)

pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)

file (GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/openvino/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/base/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/openvino/*.h
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

# FIXME: implicit declaration of tracer functions
if(UNIX)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-error=implicit-function-declaration)
endif()

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/utils/
    ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/include/inference_backend/
    ${CMAKE_SOURCE_DIR}/src/monolithic/inference_backend/image_inference/openvino/
    ${CMAKE_CURRENT_SOURCE_DIR}/base/
    ${CMAKE_CURRENT_SOURCE_DIR}/openvino/
    ${CMAKE_CURRENT_SOURCE_DIR}/gvaaudiodetect/
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTALLOC_INCLUDE_DIRS}
    utils
)

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTALLOC_LIBRARIES}
    dlstreamer_api
    common
    inference_backend
    inference_elements
    image_inference_openvino
    logger
    openvino::runtime
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
