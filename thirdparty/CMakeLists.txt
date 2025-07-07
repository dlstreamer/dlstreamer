# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

add_subdirectory(itt)

if(${ENABLE_TESTS})
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/googletest/CMakeLists.txt)
        message("Using thirdparty/googletest")
        add_subdirectory(googletest)
    else()
        find_package(GTest) # sudo apt install libgtest-dev libgmock-dev
        if (GTest_FOUND AND GTest_VERSION VERSION_GREATER_EQUAL "1.10")
            message(STATUS "Found GTest: version ${GTest_VERSION}")
            include_directories(${GTEST_INCLUDE_DIRS})
        else()
            message("GTest library not found, skipping tests build")
            SET(ENABLE_TESTS OFF CACHE BOOL "Parameter to enable tests building" FORCE)
        endif()
    endif()
    
    add_subdirectory(fff)
    set(FFF_SOURCE "https://raw.githubusercontent.com/meekrosoft/fff/v1.0/fff.h")
    set(FFF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/fff/include/")
    set(FFF_HEADER "${FFF_PATH}/fff.h")

    download_and_verify(${FFF_SOURCE} ${FFF_HEADER} "fff header")
endif()

# Populate thirdparty/json dependency
set(JSONCONVERT_SOURCE "https://github.com/nlohmann/json/releases/download/v3.7.3/json.hpp")
set(JSONCONVERT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/nlohmann")
set(JSONCONVERT_HEADER "${JSONCONVERT_PATH}/json.hpp")
set(JSONVALIDATOR_SOURCE "https://github.com/pboettch/json-schema-validator/archive/2.0.0.tar.gz")
set(JSONVALIDATOR_PATH "${CMAKE_CURRENT_SOURCE_DIR}/json/pboettch")
set(JSONVALIDATOR_TAR "${JSONVALIDATOR_PATH}/jsonvalidator.tar.gz")
set(JSONVALIDATOR_HEADER "${JSONVALIDATOR_PATH}/json-schema-validator-2.0.0/src/json-schema.hpp")

download_and_verify(${JSONCONVERT_SOURCE} ${JSONCONVERT_HEADER} "JSON header")

if(NOT EXISTS ${JSONVALIDATOR_HEADER})
    download_and_verify(${JSONVALIDATOR_SOURCE} ${JSONVALIDATOR_TAR} "JSON validator")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf ${JSONVALIDATOR_TAR}
        WORKING_DIRECTORY ${JSONVALIDATOR_PATH}
    )
    # fix build issue
    execute_process(
        COMMAND sed -i "s/&c : s/c : s/g" json-schema-validator-2.0.0/src/json-validator.cpp
        WORKING_DIRECTORY ${JSONVALIDATOR_PATH}
    )
endif()

add_subdirectory(json)

# Hint compiler about location of the header file generated.
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(spdlog)