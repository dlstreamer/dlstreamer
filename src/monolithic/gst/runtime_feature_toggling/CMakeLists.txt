# ==============================================================================
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set(TARGET_NAME "runtime_feature_toggling")

set(TOGGLING_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime_feature_toggler.h
    ${CMAKE_CURRENT_SOURCE_DIR}/environment_variable_options_reader.h
)

set(TOGGLING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime_feature_toggler.cpp
)

add_library(${TARGET_NAME} STATIC ${TOGGLING_HEADERS} ${TOGGLING_SOURCES})

target_link_libraries(${TARGET_NAME}
PUBLIC
inference_backend
utils
)

target_include_directories(${TARGET_NAME}
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
