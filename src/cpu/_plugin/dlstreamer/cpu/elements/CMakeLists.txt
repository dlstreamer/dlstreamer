# ==============================================================================
# Copyright (C) 2022-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "dlstreamer_cpu")
set(SOURCES ${MAIN_SRC} elements.cpp)
set(LIBS
    dlstreamer_api
    rate_adjust
    meta_smooth
    tensor_convert
    tensor_histogram
    tensor_sliding_window
    tensor_postproc
)

add_library(${TARGET_NAME} SHARED ${SOURCES})
add_library(${TARGET_NAME}_static STATIC ${SOURCES})

foreach(target ${TARGET_NAME} ${TARGET_NAME}_static)
    set_compile_flags(${target})
    target_link_libraries(${target} PRIVATE ${LIBS})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${TARGET_NAME})
    install(TARGETS ${target} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
endforeach()