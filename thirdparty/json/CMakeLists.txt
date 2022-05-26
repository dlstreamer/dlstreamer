# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_subdirectory(nlohmann)
# Disable installation
add_subdirectory(pboettch EXCLUDE_FROM_ALL ON)
