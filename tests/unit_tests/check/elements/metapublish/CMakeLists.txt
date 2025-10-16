# ==============================================================================
# Copyright (C) 2018-2025 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

add_subdirectory(test_metapublish)
add_subdirectory(test_properties)

if(${ENABLE_RDKAFKA_INSTALLATION})
    add_subdirectory(test_metapublish_kafka)
endif()
