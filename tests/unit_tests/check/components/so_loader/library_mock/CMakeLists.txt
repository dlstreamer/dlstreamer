# ==============================================================================
# Copyright (C) 2021-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

project("librarymock")

add_library("librarymock1" SHARED librarymock1.cpp)
add_library("librarymock2" SHARED librarymock2.cpp)

set_target_properties("librarymock1" PROPERTIES
LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)
set_target_properties("librarymock2" PROPERTIES
LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)