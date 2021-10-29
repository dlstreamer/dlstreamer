# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(itt)
if(${ENABLE_VAS_TRACKER} AND ${DOWNLOAD_VAS_TRACKER})
    add_subdirectory(vas)
endif()

# Populate thirdparty/json dependency
set(JSONCONVERT_SOURCE "https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp")
set(JSONCONVERT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/nlohmann")
set(JSONCONVERT_HEADER "${JSONCONVERT_PATH}/json.hpp")
set(JSONVALIDATOR_SOURCE "https://github.com/pboettch/json-schema-validator/archive/2.0.0.tar.gz")
set(JSONVALIDATOR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/pboettch")
set(JSONVALIDATOR_TAR "${JSONVALIDATOR_PATH}/jsonvalidator.tar.gz")
set(JSONVALIDATOR_HEADER "${JSONVALIDATOR_PATH}/json-schema-validator-2.0.0/src/json-schema.hpp")

if(NOT EXISTS ${JSONCONVERT_HEADER})
    file(DOWNLOAD ${JSONCONVERT_SOURCE} ${JSONCONVERT_HEADER} SHOW_PROGRESS)
endif()

if(NOT EXISTS ${JSONVALIDATOR_HEADER})
    if(NOT EXISTS ${JSONVALIDATOR_TAR})
        file(DOWNLOAD ${JSONVALIDATOR_SOURCE} ${JSONVALIDATOR_TAR} SHOW_PROGRESS)
    endif()

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${JSONVALIDATOR_TAR}
        WORKING_DIRECTORY ${JSONVALIDATOR_PATH}
    )
endif()

add_subdirectory(json)

# Hint compiler about location of the header file generated.
include_directories(${CMAKE_CURRENT_BINARY_DIR})
