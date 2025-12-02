# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.5)

set(TARGET_NAME "json-hpp")

add_library(${TARGET_NAME} INTERFACE)
get_filename_component(json_dir ../ ABSOLUTE)

target_include_directories(${TARGET_NAME} INTERFACE ${json_dir})
