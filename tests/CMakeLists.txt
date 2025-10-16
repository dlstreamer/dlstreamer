# ==============================================================================
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

add_subdirectory(unit_tests)
if(${ENABLE_FUZZING})
    add_subdirectory(fuzzing)
endif()
