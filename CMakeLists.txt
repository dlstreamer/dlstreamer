# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

project(DL_Streamer)

if(NOT(UNIX) AND NOT(WIN32))
    message(FATAL_ERROR "Only UNIX and Windows supported")
endif()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is undefined. Set default build type ${CMAKE_BUILD_TYPE}.")
endif()

# Define version

set(VERSION_MAJOR 1)
set(VERSION_MINOR 6)
set(VERSION_PATCH 0)

if (NOT DEFINED GIT_INFO OR "${GIT_INFO}" STREQUAL "")
    set(GIT_INFO "0")
    message(WARNING "GIT_INFO is undefined. Set default value ${GIT_INFO}.")
endif()

configure_file(cmake/version.txt.in version.txt)

set(PRODUCT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(PLUGIN_VERSION ${PRODUCT_VERSION}.${GIT_INFO})

# Propagate version to plugins

add_definitions(-DPLUGIN_VERSION="${PLUGIN_VERSION}")
add_definitions(-DPACKAGE="gst-video-analytics")
add_definitions(-DPACKAGE_NAME="DL Streamer elements")
add_definitions(-DGST_PACKAGE_ORIGIN="https://github.com/openvinotoolkit/dlstreamer_gst")
add_definitions(-DPLUGIN_LICENSE="MIT/X11")

# Helper macros and functions

macro(find_IE_package)
    set(IE_MIN_VERSION "2021.4.0")
    find_package(InferenceEngine REQUIRED)
    if (${InferenceEngine_VERSION} VERSION_EQUAL "0.0.0")
        message(WARNING "InferenceEngine version is 0.0.0. If you built OpenVINO from source, you can skip this warning")
    else()
        if (${InferenceEngine_VERSION} VERSION_LESS ${IE_MIN_VERSION})
            message(FATAL_ERROR "InferenceEngine version should be >= ${IE_MIN_VERSION}. Found: ${InferenceEngine_VERSION}")
        endif()
    endif()
endmacro(find_IE_package)

macro(set_target_lib_version TARGET)
    set_target_properties(
        ${TARGET}
        PROPERTIES
        SOVERSION ${VERSION_MAJOR}
        VERSION ${PRODUCT_VERSION}
    )
endmacro(set_target_lib_version)

function (set_compile_flags TARGET)
    target_compile_options(${TARGET} PRIVATE $<$<COMPILE_LANGUAGE:C>:${C_FLAGS}> $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS}>)
endfunction(set_compile_flags)

# Set output paths

if (NOT(BIN_FOLDER))
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        set(BIN_FOLDER intel64)
    else()
        set(BIN_FOLDER ia32)
    endif()
endif()

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (CMAKE_PLUGIN_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib/gstreamer-1.0)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/lib)
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/bin)
set (CMAKE_SAMPLES_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${BIN_FOLDER}/${CMAKE_BUILD_TYPE}/samples)

set (DLSTREAMER_LIBRARIES_INSTALL_PATH lib)
set (DLSTREAMER_PLUGINS_INSTALL_PATH ${DLSTREAMER_LIBRARIES_INSTALL_PATH}/gstreamer-1.0)
set (DLSTREAMER_HEADERS_INSTALL_PATH include)

# include and lib folders of this project
set (DLSTREAMER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/gst-libs)
set (DLSTREAMER_LIBRARIES ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

# Options

include(CMakeDependentOption)

cmake_dependent_option(TREAT_WARNING_AS_ERROR "Treat build warnings as errors" ON "UNIX" OFF)

cmake_dependent_option(ENABLE_ITT "Enable ITT for tracing" ON "UNIX AND NOT ${CMAKE_SYSTEM_PROCESSOR} STREQUAL aarch64" OFF)

cmake_dependent_option(DOWNLOAD_VAS_TRACKER "Download VAS tracker" ON "UNIX AND NOT ${CMAKE_SYSTEM_PROCESSOR} STREQUAL aarch64" OFF)
cmake_dependent_option(ENABLE_VAS_TRACKER "Enables vas tracker as part of gvatrack" ON "UNIX" OFF)
if (NOT ${ENABLE_VAS_TRACKER})
    message("ENABLE_VAS_TRACKER is set to OFF. gvatrack element won't be available")
endif()

cmake_dependent_option(ENABLE_VAAPI "Parameter to enable VAAPI for image pre-processing" OFF "UNIX" OFF)

option(ENABLE_VPUX "Enables VPUX specific features" OFF)

cmake_dependent_option(ENABLE_PAHO_INSTALLATION "Enables paho-mqtt3c installation" OFF "UNIX" OFF)
cmake_dependent_option(ENABLE_RDKAFKA_INSTALLATION "Enables rdkafka installation" OFF "UNIX" OFF)
message("ENABLE_PAHO_INSTALLATION=${ENABLE_PAHO_INSTALLATION}")
message("ENABLE_RDKAFKA_INSTALLATION=${ENABLE_RDKAFKA_INSTALLATION}")

option(ENABLE_AUDIO_INFERENCE_ELEMENTS "Enables audio inference elements" ON)

option(ENABLE_SAMPLES "Parameter to enable samples building" ON)

# Define compiler flags

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2")

    set (CMAKE_C_STANDARD 11)
    set (CMAKE_CXX_STANDARD 20)
else()
    # Common compilation flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuninitialized -Winit-self -Wmaybe-uninitialized -Warray-bounds -fstack-protector-strong -Wno-deprecated-declarations  -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wuninitialized -Warray-bounds -fstack-protector-strong -Wno-deprecated-declarations -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv")

    if(TREAT_WARNING_AS_ERROR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
    endif()

    # Additional compilation flags aplied to specific targets
    set(C_FLAGS -Wall -Wextra)
    set(CXX_FLAGS -Wall -Wextra)

    set(CMAKE_CXX_FLAGS_RELEASE "-O2")
    if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
        # TODO: test if it increases x86 performance as well and rm check for aarch64
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize") # should be used with "-O2"
    endif()

    ## to use C/C++11
    set (CMAKE_C_STANDARD 11)
    set (CMAKE_CXX_STANDARD 11)
    set (CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUDIO ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAUDIO ")
endif()

# Create config file

configure_file(cmake/config.h.in configs/config.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/configs)

# Code coverage

# Find common packages

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER gstreamer-1.0>=1.16 REQUIRED)
link_directories(${GSTREAMER_LIBRARY_DIRS})

# Add subdirectories

add_subdirectory(gst)
add_subdirectory(gst-libs)
add_subdirectory(inference_backend)
add_subdirectory(thirdparty)
add_subdirectory(utils)

if(${ENABLE_SAMPLES})
    add_subdirectory(samples)
endif()
