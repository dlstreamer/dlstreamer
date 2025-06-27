# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "gstdlstreamer_opencl")

file(GLOB_RECURSE ELEMENTS_CPP_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../*/elements.cpp")

add_library(${TARGET_NAME} SHARED register_elements.cpp ${ELEMENTS_CPP_FILE})
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
PRIVATE
    dlstreamer_opencl
    dlstreamer_gst
    opencl_tensor_normalize
    vaapi_to_opencl
)

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
