# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

cmake_minimum_required(VERSION 3.1)

add_definitions(-DMICRO_ROI_NO_SPLIT)

add_subdirectory(common)
add_subdirectory(gvatensoracc)
add_subdirectory(gvatensorinference)
add_subdirectory(gvatensortometa)
add_subdirectory(gvavideototensor)
add_subdirectory(tensormux)
add_subdirectory(bins)
add_subdirectory(registrator)
