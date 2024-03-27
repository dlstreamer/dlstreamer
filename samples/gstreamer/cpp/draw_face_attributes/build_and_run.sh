#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

FILE=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}
OUTPUT=${2:-display} # Valid values: display, display-and-json, json

BASE_DIR="$(realpath "$(dirname "$0")")"
BUILD_DIR=$HOME/intel/dl_streamer/samples/draw_face_attributes/build
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}" || exit

if [ -f /etc/lsb-release ]; then
    cmake "${BASE_DIR}"
else
    cmake3 "${BASE_DIR}"
fi

make -j "$(nproc)"

"${BUILD_DIR}"/draw_face_attributes -i "$FILE" -o "$OUTPUT"
