# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "elements")

find_package(OpenCV REQUIRED core imgproc)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.16 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)
if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
        pkg_check_modules(GSTAUDIO gstreamer-audio-1.0>=1.16 REQUIRED)
endif()

file (GLOB MAIN_SRC
    gvametaconvert/*.cpp
    gvametaconvert/*.c
    gvametapublish/*.cpp
    gvametapublish/*.c
    gvawatermark/*.cpp
    gvawatermark/renderer/*.cpp
    gvawatermark/*.c
    gvafpscounter/*.cpp
    gvafpscounter/*.c
    gvametaaggregate/*.cpp
    gvametaaggregate/*.c
)

file (GLOB MAIN_HEADERS
    gvametaconvert/*.h
    gvametapublish/*.h
    gvawatermark/*.h
    gvawatermark/renderer/*.h
    gvafpscounter/*.h
    gvametaaggregate/*.h
)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    file (GLOB AUDIO_SRC
        gvametaconvert/audioconverter/*.cpp
    )
    file (GLOB AUDIO_HEADERS
        gvametaconvert/audioconverter/*.h
    )
    list (APPEND MAIN_SRC ${AUDIO_SRC})
    list (APPEND MAIN_HEADERS ${AUDIO_HEADERS})
endif()

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    gvametaconvert
    gvametapublish
    gvawatermark
    gvafpscounter
    gvametaaggregate
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${GSTALLOC_INCLUDE_DIRS}
)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/gvametaconvert/audioconverter
        ${GSTAUDIO_INCLUDE_DIRS}
    )
endif()
target_link_libraries(${TARGET_NAME}
PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GSTALLOC_LIBRARIES}
    ${OpenCV_LIBS}
    common
    image_inference
    image_inference_openvino
    logger
    json-hpp
    json-schema-validator
    utils
)

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${GSTAUDIO_LIBRARIES})
endif()

if(${ENABLE_VAAPI})
        target_link_libraries(${TARGET_NAME} PRIVATE image_inference_async)
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
        gvametapublish
)
if (MQTT)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPAHO_INC ")
        target_link_libraries(${TARGET_NAME} PRIVATE paho-mqtt3a uuid)
        message("MQTT support will be available in metapublish element")
else()
        message("MQTT support will not be available. Ensure required libraries (Paho MQTT) are installed, and rebuild plugin with -DMQTT=1 before attempting to use MQTT")
endif()

if (KAFKA)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DKAFKA_INC ")
        target_link_libraries(${TARGET_NAME} PRIVATE rdkafka)
        message("Kafka support will be available in metapublish element")
else()
        message("Kafka support will not be available. Ensure required libraries (librdkafka) are installed, and rebuild plugin with -DKAFKA=1 before attempting to use Kafka")
endif()

add_subdirectory(gvapython)
if(${ENABLE_VAS_TRACKER})
    add_subdirectory(gvatrack)
endif()