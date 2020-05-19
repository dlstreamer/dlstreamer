# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(itt)

if(${ENABLE_VAS_TRACKER})
    add_subdirectory(vas)
    include(${CMAKE_CURRENT_SOURCE_DIR}/vas/DownloadVas.cmake)
    add_dependencies(vas getexternalvas)
endif()

if(${ENABLE_VAS_TRACKER})
    add_subdirectory(vas)
    include(${CMAKE_CURRENT_SOURCE_DIR}/vas/DownloadVas.cmake)
    add_dependencies(vas getexternalvas)
endif()

# Populate thirdparty/json dependency
set(JSONCONVERT_SOURCE "https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp")
set(JSONCONVERT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/nlohmann")
set(JSONCONVERT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/json/nlohmann/json.hpp")
set(JSONVALIDATOR_SOURCE "https://github.com/pboettch/json-schema-validator/archive/2.0.0.tar.gz")
set(JSONVALIDATOR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/pboettch")
set(JSONVALIDATOR_TAR "${CMAKE_CURRENT_SOURCE_DIR}/json/pboettch/jsonvalidator.tar.gz")
set(JSONVALIDATOR_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/json/pboettch/json-schema-validator-2.0.0/src/json-schema.hpp")
# Allow wget up to 90 seconds and 1 retry
set(WGET_TIMEOUT_SECONDS 90)
set(WGET_RETRY_COUNT 1)

set_source_files_properties(${JSONCONVERT_HEADER} PROPERTIES EXTERNAL_OBJECT TRUE)

add_custom_command(
	OUTPUT ${JSONCONVERT_HEADER}
    COMMAND echo "Downloading ${JSONCONVERT_HEADER}..."
    COMMAND mkdir -p ${JSONCONVERT_PATH} && wget --tries=${WGET_RETRY_COUNT} --timeout=${WGET_TIMEOUT_SECONDS} ${JSONCONVERT_SOURCE} --output-document=${JSONCONVERT_HEADER}
    COMMAND touch ${JSONCONVERT_HEADER}
)

add_custom_target(getexternal
    DEPENDS "${JSONCONVERT_HEADER}"
)
if(NOT EXISTS ${JSONVALIDATOR_TAR})
    file(DOWNLOAD ${JSONVALIDATOR_SOURCE} ${JSONVALIDATOR_TAR})
endif()
if(NOT EXISTS ${JSONVALIDATOR_HEADER})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${JSONVALIDATOR_TAR}
        WORKING_DIRECTORY ${JSONVALIDATOR_PATH}
    )
endif()

add_subdirectory(json)

# Recognize JSONCONVERT_HEADER as a dependency, add build trigger
add_dependencies(json-hpp getexternal)

# Hint compiler about location of the header file generated.
include_directories(${CMAKE_CURRENT_BINARY_DIR})
