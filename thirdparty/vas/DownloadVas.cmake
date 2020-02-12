# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(VAS_THIRDPARTY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vas")

set(EXTRACT_OS_NAME "grep '^NAME=\".*\"' /etc/os-release | sed '1 s/^NAME=\"\\(.*\\)\"$/\\1/'")

set(BASE_URL "https://downloadmirror.intel.com/29360/eng")

execute_process(
    COMMAND bash -c ${EXTRACT_OS_NAME}
    OUTPUT_VARIABLE DIS)
if("${DIS}" MATCHES "Ubuntu")
    set(ARCHIVE_NAME "vasot.2020.1.ubuntu.tar.xz")
    set(LIBVASOTSO_LINK "${BASE_URL}/${ARCHIVE_NAME}")
else()
    set(ARCHIVE_NAME "vasot.2020.1.centos.tar.xz")
    set(LIBVASOTSO_LINK "${BASE_URL}/${ARCHIVE_NAME}")
endif()

set(WGET_TIMEOUT_SECONDS 90)
set(WGET_RETRY_COUNT 1)

set(OT_HEADER_FILE "${VAS_THIRDPARTY_ROOT_DIR}/include/vas/ot.h")
set(COMMON_HEADER_FILE "${VAS_THIRDPARTY_ROOT_DIR}/include/vas/common.h")

set(LIBVASOTSO_FILE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libvasot.so" )
set(VAS_LICENSE_FILE "${VAS_THIRDPARTY_ROOT_DIR}/Intel-Simplified-Software-License.txt")

set_source_files_properties(${COMMON_HEADER_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)
set_source_files_properties(${OT_HEADER_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)
set_source_files_properties(${VAS_LICENSE_FILE} PROPERTIES EXTERNAL_OBJECT TRUE)

add_custom_command(
    OUTPUT ${COMMON_HEADER_FILE} ${OT_HEADER_FILE} ${LIBVASOTSO_FILE} ${VAS_LICENSE_FILE}
    COMMAND echo "Downloading Vas..."
    COMMAND cd ${VAS_THIRDPARTY_ROOT_DIR}/ && wget --tries=${WGET_RETRY_COUNT} --timeout=${WGET_TIMEOUT_SECONDS} ${LIBVASOTSO_LINK}
    COMMAND tar -xJf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${VAS_THIRDPARTY_ROOT_DIR} include/vas/
    COMMAND tar --strip=2 -xJf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} lib/intel64/
    COMMAND tar --strip=1 -xJf ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME} -C ${VAS_THIRDPARTY_ROOT_DIR} license/
    COMMAND rm ${VAS_THIRDPARTY_ROOT_DIR}/${ARCHIVE_NAME}
)
add_custom_target(getexternalvas
    DEPENDS "${COMMON_HEADER_FILE}" "${OT_HEADER_FILE}" "${LIBVASOTSO_FILE}" "${VAS_LICENSE_FILE}"
)

install(FILES ${LIBVASOTSO_FILE} DESTINATION lib/gst-video-analytics)
