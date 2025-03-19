# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================


set(TARGET_NAME "capsrelax")

add_library(${TARGET_NAME} STATIC capsrelax.h capsrelax.c)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC
        dlstreamer_gst
        )

install(TARGETS ${TARGET_NAME} DESTINATION ${DLSTREAMER_PLUGINS_INSTALL_PATH})
