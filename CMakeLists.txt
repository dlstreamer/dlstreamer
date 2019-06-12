# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

project(VA_GStreamer_Plugins)

if(NOT(UNIX))
    message(FATAL_ERROR "Only UNIX supported")
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is undefined. Set default build type ${CMAKE_BUILD_TYPE}.")
endif()

set(VERSION_MAJOR 0)
set(VERSION_MINOR 4)

if (NOT DEFINED VERSION_PATCH)
    set(VERSION_PATCH 0)
    message(WARNING "VERSION_PATCH is undefined. Set default value ${VERSION_PATCH}.")
endif()

if (NOT DEFINED GIT_INFO)
    set(GIT_INFO "git_unknown")
    message(WARNING "GIT_INFO is undefined. Set default value ${GIT_INFO}.")
endif()

set(PRODUCT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(PLUGIN_VERSION ${PRODUCT_VERSION}.${GIT_INFO})
# Propagate version to plugins
add_definitions(-DPLUGIN_VERSION="${PLUGIN_VERSION}")
add_definitions(-DPACKAGE="gst-video-analytics")
add_definitions(-DPACKAGE_NAME="GStreamer Video Analytics elements")
add_definitions(-DGST_PACKAGE_ORIGIN="https://github.com/opencv/gst-video-analytics")
add_definitions(-DPLUGIN_LICENSE="MIT/X11")

macro(set_target_lib_version TARGET)
    set_target_properties(
        ${TARGET}
        PROPERTIES
        SOVERSION ${VERSION_MAJOR}
        VERSION ${PRODUCT_VERSION}
    )
endmacro(set_target_lib_version)

if (NOT(BIN_FOLDER))
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set (ARCH intel64)
    else()
        set (ARCH ia32)
    endif()
    set (BIN_FOLDER ${ARCH})
endif()

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/bin)
set (CMAKE_DOCDIR ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/doc)

# Workaround for OpenVINO R1
find_package(Threads REQUIRED)

# Common compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wuninitialized -Winit-self -Wmaybe-uninitialized -Warray-bounds -fstack-protector-strong")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -Wuninitialized -Warray-bounds -fstack-protector-strong")

# Additional compilation flags aplied to specific targets
set(C_FLAGS -Wall -Wextra)
set(CXX_FLAGS -Wall -Wextra)

function (set_compile_flags TARGET)
    target_compile_options(${TARGET} PRIVATE $<$<COMPILE_LANGUAGE:C>:${C_FLAGS}> $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS}>)
endfunction(set_compile_flags)

####################################
## to use C++11
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
####################################

set(CMAKE_CXX_FLAGS_RELEASE "-O2")

###############################################################################
option(HAVE_GAPI "Parameter to enable GAPI for image preprocessing" OFF)
option(DISABLE_SAMPLES "Parameter to disable samples building" OFF)
option(ENABLE_DOCUMENTATION "Parameter to enable documentation generation" OFF)

find_library(MQTT paho-mqtt3c)
find_library(KAFKA rdkafka)

configure_file(cmake/config.h.in configs/config.h @ONLY)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/configs)

add_subdirectory(gst)
add_subdirectory(gst-libs)
add_subdirectory(inference_backend)
add_subdirectory(thirdparty)

if(NOT ${DISABLE_SAMPLES})
    add_subdirectory(samples)
endif()
