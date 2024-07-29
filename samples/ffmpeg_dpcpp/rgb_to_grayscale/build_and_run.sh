#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

VIDEO_FILE=$(realpath "$1")
EXE_NAME=ffmpeg_dpcpp_rgb_to_grayscale
BUILD_DIR=$HOME/intel/dl_streamer/samples/${EXE_NAME}/build
BASE_DIR=$(realpath "$(dirname "$0")")

# shellcheck source=/dev/null
source /opt/intel/oneapi/setvars.sh

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}" || exit

if [ -f /etc/lsb-release ]; then
    cmake "${BASE_DIR}"
else
    cmake3 "${BASE_DIR}"
fi

make -j "$(nproc)"

"${BUILD_DIR}"/${EXE_NAME} -i "${VIDEO_FILE}" -o output.gray
