# ==============================================================================
# Copyright (C) 2022 Intel Corporation
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
    tensor_convert
    tensor_histogram
    tensor_sliding_aggregate
    tensor_postproc
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
