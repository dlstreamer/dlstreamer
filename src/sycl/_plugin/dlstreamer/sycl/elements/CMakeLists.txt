# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_sycl")

add_library(${TARGET_NAME} SHARED ${MAIN_SRC} elements.cpp)
set_compile_flags(${TARGET_NAME})

pkg_check_modules(LEVEL_ZERO libze_loader)

target_link_libraries(${TARGET_NAME}
PRIVATE
    dlstreamer_api
    ${LEVEL_ZERO_LIBRARIES}
    sycl_tensor_histogram
    sycl_meta_overlay
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
