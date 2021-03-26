# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

set(TARGET_NAME "vas")

set(VAS_THIRDPARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(EXTRACT_OS_NAME "grep '^NAME=\".*\"' /etc/os-release | sed '1 s/^NAME=\"\\(.*\\)\"$/\\1/'")

set(BASE_URL "https://downloadmirror.intel.com/30258/eng")

execute_process(
    COMMAND bash -c ${EXTRACT_OS_NAME}
    OUTPUT_VARIABLE DIS)
if("${DIS}" MATCHES "Ubuntu")
    set(ARCHIVE_NAME "vasot.2021.3.ubuntu18.opencv.4.5.2.tar.xz")
    set(LIBVASOT_ARCHIVE_URL "${BASE_URL}/${ARCHIVE_NAME}")
else()
    set(ARCHIVE_NAME "vasot.2021.3.centos7.opencv.4.5.2.tar.xz")
    set(LIBVASOT_ARCHIVE_URL "${BASE_URL}/${ARCHIVE_NAME}")
endif()

set(WGET_TIMEOUT_SECONDS 90)
set(WGET_RETRY_COUNT 5)

set(OT_HEADER_FILE "${VAS_THIRDPARTY_ROOT_DIR}/include/vas/ot.h")
set(COMMON_HEADER_FILE "${VAS_THIRDPARTY_ROOT_DIR}/include/vas/common.h")

set(LIBVASOTSO_BASE "libvasot.so" )
set(LIBVASOTSO_FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${LIBVASOTSO_BASE}" )
set(VAS_LICENSE_FILE "${VAS_THIRDPARTY_ROOT_DIR}/Intel-Simplified-Software-License.txt")

set_source_files_properties(${COMMON_HEADER_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)
set_source_files_properties(${OT_HEADER_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)
set_source_files_properties(${VAS_LICENSE_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)

add_custom_command(
    OUTPUT ${COMMON_HEADER_FILE} ${OT_HEADER_FILE} ${LIBVASOTSO_FILE} ${VAS_LICENSE_FILE}
    COMMAND echo "Downloading Vas..."
    COMMAND cd ${VAS_THIRDPARTY_ROOT_DIR}/ && wget --tries=${WGET_RETRY_COUNT} --timeout=${WGET_TIMEOUT_SECONDS} ${LIBVASOT_ARCHIVE_URL}
    COMMAND tar -xJf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${VAS_THIRDPARTY_ROOT_DIR} include/vas/
    COMMAND tar --strip=2 -xJf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} lib/intel64/
    COMMAND tar --strip=1 -xJf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${VAS_THIRDPARTY_ROOT_DIR} license/
    COMMAND rm ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME}
)

add_custom_target(getexternalvas
    DEPENDS "${LIBVASOTSO_FILE}" "${COMMON_HEADER_FILE}" "${OT_HEADER_FILE}" "${LIBVASOTSO_FILE}" "${VAS_LICENSE_FILE}"
)

install(FILES ${LIBVASOTSO_FILE} DESTINATION ${DLSTREAMER_LIBRARIES_INSTALL_PATH})


add_library(${TARGET_NAME} INTERFACE)

target_include_directories(${TARGET_NAME} INTERFACE include)
target_link_libraries(${TARGET_NAME}
INTERFACE
    ${LIBVASOTSO_FILE}
)

add_dependencies(${TARGET_NAME} getexternalvas)
