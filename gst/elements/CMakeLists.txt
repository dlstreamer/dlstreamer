# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

set (TARGET_NAME "elements")

find_package(OpenCV REQUIRED core imgproc)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER gstreamer-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTVIDEO gstreamer-video-1.0>=1.14 REQUIRED)
pkg_check_modules(GSTALLOC gstreamer-allocators-1.0 REQUIRED)

file (GLOB MAIN_SRC
    gvaidentify/*.cpp
    gvaidentify/*.c
    gvametaconvert/*.cpp
    gvametaconvert/*.c
    gvametapublish/*.cpp
    gvametapublish/*.c
    gvawatermark/*.cpp
    gvawatermark/*.c
    gvafpscounter/*.cpp
    gvafpscounter/*.c
    gvaspeedometer/*.c
    gvaspeedometer/*.cpp
    gvatrack/*.cpp
    gvatrack/*.c
    gvatrack/iou/*.cpp
)

file (GLOB MAIN_HEADERS
    gvaidentify/*.h
    gvametaconvert/*.h
    gvametapublish/*.h
    gvawatermark/*.h
    gvafpscounter/*.h
    gvaspeedometer/*.h
    gvatrack/*.h
    gvatrack/iou/*.h
)

if(${ENABLE_VAS_TRACKER})
    file (GLOB VAS_SRC
        gvatrack/vas/*.cpp
    )
    file (GLOB VAS_HEADERS
        gvatrack/vas/*.h
    )
    list (APPEND MAIN_SRC ${VAS_SRC})
    list (APPEND MAIN_HEADERS ${VAS_HEADERS})
endif()

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    gvaidentify
    gvametaconvert
    gvametapublish
    gvawatermark
    gvafpscounter
    gvaspeedometer
    gvatrack
    gvatrack/iou
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${GSTALLOC_INCLUDE_DIRS}
)

if(${ENABLE_VAS_TRACKER})
    target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/gvatrack/vas
    )

    target_link_libraries(${TARGET_NAME}
    PRIVATE
        vas
    )
endif()

target_link_libraries(${TARGET_NAME}
PRIVATE
    ${OpenCV_LIBS}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GSTALLOC_LIBRARIES}
    common
    image_inference
    image_inference_openvino
    logger
    jsonconvert
)

if(${HAVE_VAAPI})
        target_link_libraries(${TARGET_NAME} PRIVATE image_inference_async)
endif()

target_include_directories(${TARGET_NAME}
PRIVATE
        gvametapublish
)
if (MQTT)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPAHO_INC ")
        target_link_libraries(${TARGET_NAME} PRIVATE paho-mqtt3c uuid)
        target_compile_definitions(${TARGET_NAME} PRIVATE META_PUBLISH_MQTT=1)
        message("MQTT support will be available in metapublish element")
else()
        target_compile_definitions(${TARGET_NAME} PRIVATE META_PUBLISH_MQTT=0)
        message("MQTT support will not be available. Ensure required libraries (Paho MQTT) are installed, and rebuild plugin with -DMQTT=1 before attempting to use MQTT")
endif()

if (KAFKA)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DKAFKA_INC ")
        target_link_libraries(${TARGET_NAME} PRIVATE rdkafka)
        target_compile_definitions(${TARGET_NAME} PRIVATE META_PUBLISH_KAFKA=1)
        message("Kafka support will be available in metapublish element")
else()
        target_compile_definitions(${TARGET_NAME} PRIVATE META_PUBLISH_KAFKA=0)
        message("Kafka support will not be available. Ensure required libraries (librdkafka) are installed, and rebuild plugin with -DKAFKA=1 before attempting to use Kafka")
endif()

add_subdirectory(gvapython)
