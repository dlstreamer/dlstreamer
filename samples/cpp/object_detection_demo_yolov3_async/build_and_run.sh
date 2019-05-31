#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

BASEDIR=$(dirname "$0")/../../..

if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi


FILE=${1:-$BASEDIR/../video-examples/Pexels_Videos_4786.h264}

$BASEDIR/scripts/build.sh && \
$BASEDIR/build/intel64/Release/bin/object_detection_demo_yolov3_async \
    -i $FILE
