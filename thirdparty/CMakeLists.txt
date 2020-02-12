# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(itt)
add_subdirectory(json)

if(${ENABLE_VAS_TRACKER})
    add_subdirectory(vas)
    include(${CMAKE_CURRENT_SOURCE_DIR}/vas/DownloadVas.cmake)
    add_dependencies(vas getexternalvas)
endif()

# Populate thirdparty/json dependency
set(JSONCONVERT_SOURCE "https://github.com/nlohmann/json/releases/download/v3.5.0/json.hpp")
set(JSONCONVERT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/include/")
set(JSONCONVERT_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/json/include/json.hpp")
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

# Recognize JSONCONVERT_HEADER as a dependency, add build trigger
add_dependencies(jsonconvert getexternal)

# Hint compiler about location of the header file generated.
include_directories(${CMAKE_CURRENT_BINARY_DIR})
