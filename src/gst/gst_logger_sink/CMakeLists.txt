# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

add_library(gst_logger_sink STATIC gst_logger_sink.cpp gst_logger_sink.h)

target_include_directories(gst_logger_sink
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
)
target_link_libraries(gst_logger_sink PUBLIC spdlog::spdlog PRIVATE ${GSTREAMER_LIBRARIES})
