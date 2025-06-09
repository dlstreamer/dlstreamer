# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_cpu")

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} elements.cpp)
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PRIVATE
    dlstreamer_api
    rate_adjust
    meta_smooth
    tensor_convert
    tensor_histogram
    tensor_sliding_window
    tensor_postproc
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})

add_library(${TARGET_NAME}_static STATIC ${MAIN_SRC} elements.cpp)
set_compile_flags(${TARGET_NAME}_static)

target_link_libraries(${TARGET_NAME}_static
PRIVATE
    dlstreamer_api
    rate_adjust
    meta_smooth
    tensor_convert
    tensor_histogram
    tensor_sliding_window
    tensor_postproc
)

set_target_properties(${TARGET_NAME}_static PROPERTIES OUTPUT_NAME ${TARGET_NAME})
install(TARGETS ${TARGET_NAME}_static DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
