# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
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

option(TREAT_WARNING_AS_ERROR "Treat build warnings as errors" OFF)

set(VERSION_MAJOR 1)
set(VERSION_MINOR 4)

if (NOT DEFINED VERSION_PATCH OR "${VERSION_PATCH}" STREQUAL "")
    set(VERSION_PATCH 0)
    message(WARNING "VERSION_PATCH is undefined. Set default value ${VERSION_PATCH}.")
endif()

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
add_definitions(-DPACKAGE_NAME="GStreamer Video Analytics elements")
add_definitions(-DGST_PACKAGE_ORIGIN="https://github.com/openvinotoolkit/dlstreamer_gst")
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

find_package(Threads REQUIRED)

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

function (set_compile_flags TARGET)
    target_compile_options(${TARGET} PRIVATE $<$<COMPILE_LANGUAGE:C>:${C_FLAGS}> $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS}>)
endfunction(set_compile_flags)

####################################
## to use C/C++11
set (CMAKE_C_STANDARD 11)
set (CMAKE_CXX_STANDARD 11)
set (CMAKE_POSITION_INDEPENDENT_CODE ON)
####################################

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    # TODO: test if it increases x86 performance as well and rm check for aarch64
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -ftree-vectorize") # should be used with "-O2"
endif()

if (ENABLE_CODE_COVERAGE)
    include(${CMAKE_CURRENT_SOURCE_DIR}/CodeCoverage.cmake)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_COVERAGE}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_COVERAGE}")
    set(COVERAGE_GCOVR_EXCLUDES samples thirdparty tests)
    set(COVERAGE_COMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/code_coverage_command.sh)
    set(COVERAGE_COMAND_ARGS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
    SETUP_TARGET_FOR_COVERAGE_GCOVR_HTML(NAME code_coverage EXECUTABLE ${COVERAGE_COMAND} ${COVERAGE_COMAND_ARGS})
endif()


###############################################################################
if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "aarch64")
    option(ENABLE_ITT "Enable ITT for tracing" OFF)
    option(DOWNLOAD_VAS_TRACKER "Download VAS tracker" OFF)
else()
    option(ENABLE_ITT "Enable ITT for tracing" ON)
    option(DOWNLOAD_VAS_TRACKER "Download VAS tracker" ON)
endif()

option(ENABLE_VAS_TRACKER "Enables vas tracker as part of gvatrack" ON)
if (NOT ${ENABLE_VAS_TRACKER})
    message("ENABLE_VAS_TRACKER is set to OFF. gvatrack element won't be available")
endif()

option(HAVE_VAAPI "Parameter to enable VAAPI for image pre-processing" OFF)
option(ENABLE_VAAPI "Parameter to enable VAAPI for image pre-processing" OFF)
if(HAVE_VAAPI)
    message(WARNING "HAVE_VAAPI parameter is deprecated. ENABLE_VAAPI must be used instead")
    SET(ENABLE_VAAPI ON CACHE BOOL "Parameter to enable VAAPI for image pre-processing" FORCE)
endif()

option(ENABLE_PAHO_INSTALLATION "Enables paho-mqtt3c installation" OFF)
option(ENABLE_AUDIO_INFERENCE_ELEMENTS "Enables audio inference elements" ON)
option(ENABLE_RDKAFKA_INSTALLATION "Enables rdkafka installation" OFF)
option(USE_VPUSMM "Parameter to use VPUSMM" OFF)
option(USE_VPUMGR "Parameter to use VPUMGR" OFF)
option(ENABLE_VPUX "Enables VPUX specific features" OFF)
if(USE_VPUSMM)
    message(WARNING "USE_VPUSMM parameter is deprecated. ENABLE_VPUX is force-enabled instead")
    SET(ENABLE_VPUX ON CACHE BOOL "Enables VPUX specific features" FORCE)
endif()
if(USE_VPUMGR)
    message(WARNING "USE_VPUMGR parameter is deprecated. ENABLE_VPUX is force-enabled instead")
    SET(ENABLE_VPUX ON CACHE BOOL "Enables VPUX specific features" FORCE)
endif()


option(ENABLE_SAMPLES "Parameter to enable samples building" ON)

message("ENABLE_PAHO_INSTALLATION=${ENABLE_PAHO_INSTALLATION}")
if(${ENABLE_PAHO_INSTALLATION})
    find_library(MQTT paho-mqtt3c)
endif()

message("ENABLE_RDKAFKA_INSTALLATION=${ENABLE_RDKAFKA_INSTALLATION}")
if(${ENABLE_RDKAFKA_INSTALLATION})
    find_library(KAFKA rdkafka)
endif()

if(${ENABLE_AUDIO_INFERENCE_ELEMENTS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAUDIO ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DAUDIO ")
endif()

configure_file(cmake/config.h.in configs/config.h @ONLY)

include_directories(${CMAKE_CURRENT_BINARY_DIR}/configs)

add_subdirectory(gst)
add_subdirectory(gst-libs)
add_subdirectory(inference_backend)
add_subdirectory(thirdparty)
add_subdirectory(utils)

if(${ENABLE_SAMPLES})
    add_subdirectory(samples)
endif()
