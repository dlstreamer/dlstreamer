# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "vas")

set(VAS_THIRDPARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(EXTRACT_OS_NAME "grep '^NAME=\".*\"' /etc/os-release | sed '1 s/^NAME=\"\\(.*\\)\"$/\\1/'")
set(EXTRACT_OS_VERSION "grep '^VERSION_ID=\".*\"' /etc/os-release | sed '1 s/^VERSION_ID=\"\\(.*\\)\"$/\\1/'")

execute_process(COMMAND bash -c ${EXTRACT_OS_NAME} OUTPUT_VARIABLE OS_NAME)
execute_process(COMMAND bash -c ${EXTRACT_OS_VERSION} OUTPUT_VARIABLE OS_VERSION)

set(BASE_URL "https://downloadmirror.intel.com/682452")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    # set VASOT archive names
    if("${OS_NAME}" MATCHES "Ubuntu" AND "${OS_VERSION}" MATCHES "18.04")
        set(ARCHIVE_NAME "vas.2021.4.2.ubuntu18.04.tar.xz")
    elseif("${OS_NAME}" MATCHES "Ubuntu" AND "${OS_VERSION}" MATCHES "20.04")
        set(ARCHIVE_NAME "vas.2021.4.2.ubuntu20.04.tar.xz")
    elseif("${OS_NAME}" MATCHES "CentOS Linux" AND "${OS_VERSION}" MATCHES "7.*")
        set(ARCHIVE_NAME "vas.2021.4.2.centos7.6.tar.xz")
    elseif("${OS_NAME}" MATCHES "Red Hat Enterprise Linux" AND "${OS_VERSION}" MATCHES "8.2")
        set(ARCHIVE_NAME "vas.2021.4.2.rhel8.2.tar.xz")
    endif()
else()
    message(ERROR "gvatrack element currently does not support Windows")
endif()
set(LIBVASOT_ARCHIVE_URL "${BASE_URL}/${ARCHIVE_NAME}")

set(WGET_TIMEOUT_SECONDS 90)
set(WGET_RETRY_COUNT 5)

set(OT_HEADER_FILE "${VAS_THIRDPARTY_ROOT_DIR}/include/vas/ot.h")
set(COMMON_HEADER_FILE "${VAS_THIRDPARTY_ROOT_DIR}/include/vas/common.h")

set(LIBVASOTSO_BASE "libvasot.so")
set(LIBVASOTSO_FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${LIBVASOTSO_BASE}")
set(VAS_LICENSE_FILE "${VAS_THIRDPARTY_ROOT_DIR}/Intel-Simplified-Software-License.txt")

set_source_files_properties(${COMMON_HEADER_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)
set_source_files_properties(${OT_HEADER_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)
set_source_files_properties(${VAS_LICENSE_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)

add_custom_command(
    OUTPUT ${COMMON_HEADER_FILE} ${OT_HEADER_FILE} ${LIBVASOTSO_FILE} ${VAS_LICENSE_FILE}
    COMMAND echo "Downloading Vas..."
    COMMAND cd ${VAS_THIRDPARTY_ROOT_DIR}/ && wget --tries=${WGET_RETRY_COUNT} --timeout=${WGET_TIMEOUT_SECONDS} ${LIBVASOT_ARCHIVE_URL}
    COMMAND tar -xvf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${VAS_THIRDPARTY_ROOT_DIR} include/vas/
    COMMAND tar --strip=2 -xvf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} lib/intel64/
    COMMAND tar --strip=1 -xvf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${VAS_THIRDPARTY_ROOT_DIR} license/
)

add_custom_target(getexternalvas
    DEPENDS "${COMMON_HEADER_FILE}" "${OT_HEADER_FILE}" "${LIBVASOTSO_FILE}" "${VAS_LICENSE_FILE}"
)

add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME} INTERFACE include)

target_link_libraries(${TARGET_NAME}
INTERFACE
    ${LIBVASOTSO_FILE}
)

add_dependencies(${TARGET_NAME} getexternalvas)

install(DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/
    DESTINATION ${DLSTREAMER_LIBRARIES_INSTALL_PATH}
    FILES_MATCHING PATTERN "libvasot*.so*"
)
