#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

BASE_DIR=$PWD
BUILD_DIR=$HOME/intel/dl_streamer/samples/ffmpeg_vaapi_openvino/build

rm -rf ${BUILD_DIR}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

if [ -f /etc/lsb-release ]; then
    cmake ${BASE_DIR}
else
    cmake3 ${BASE_DIR}
fi

make -j $(nproc)

cd ${BASE_DIR}

FILE=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}
${BUILD_DIR}/ffmpeg_vaapi_openvino -i $FILE
