# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required (VERSION 3.1)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/gstgva
        DESTINATION python
        USE_SOURCE_PERMISSIONS
        PATTERN "*.pyc" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "CMakeLists.txt" EXCLUDE)
