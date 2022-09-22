# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_opencl")

add_library(${TARGET_NAME} SHARED elements.cpp)
set_compile_flags(${TARGET_NAME})

target_link_libraries(${TARGET_NAME}
    PUBLIC
        tensor_normalize_opencl
    )

if(VA_FOUND)
    target_link_libraries(${TARGET_NAME}
    PUBLIC
        vaapi_to_opencl
    )
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
