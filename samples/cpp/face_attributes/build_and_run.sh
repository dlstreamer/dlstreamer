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

$BASEDIR/scripts/build.sh

FILE=${1:-$BASEDIR/../video-examples/Fun_at_a_Fair.mp4}

$BASEDIR/build/intel64/Release/bin/face_attributes -i $FILE
