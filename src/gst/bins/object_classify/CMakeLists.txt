# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "object_classify")

file(GLOB MAIN_SRC
    object_classify.cpp
)

file(GLOB MAIN_HEADERS
    object_classify.h
)

add_library(${TARGET_NAME} STATIC ${MAIN_SRC} ${MAIN_HEADERS})
set_compile_flags(${TARGET_NAME})

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TARGET_NAME}
PUBLIC
    video_inference
)
