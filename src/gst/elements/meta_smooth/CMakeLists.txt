# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "meta_smooth")

add_library(${TARGET_NAME} STATIC meta_smooth.cpp meta_smooth.h)
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        )

target_link_libraries(${TARGET_NAME}
        PUBLIC
        dlstreamer_gst
        dlstreamer_logger
        gstvideoanalyticsmeta
        gst_logger_sink
        )
