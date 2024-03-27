#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

BUILD_TYPE=${1:-Release}

BUILD_DIR=$(dirname "$0")/../build

[ ! -d "${BUILD_DIR}" ] && mkdir "${BUILD_DIR}"
cd "${BUILD_DIR}" && cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" .. && make -j "$(nproc)"
