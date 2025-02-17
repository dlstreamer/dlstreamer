# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set (CMAKE_CXX_STANDARD 20)
add_subdirectory(nlohmann)
# Disable installation
add_subdirectory(pboettch EXCLUDE_FROM_ALL ON)
