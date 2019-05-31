#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

BASEDIR=$(dirname "$0")/../..
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh

#import GET_MODEL_PATH
source $BASEDIR/scripts/path_extractor.sh

MODEL=ssd300

DEVICE=CPU

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

FILE=${1:-$VIDEO_EXAMPLES_DIR/Pexels_Videos_4786.mp4}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} filesrc location=${FILE} ! \
               decodebin ! video/x-raw ! videoconvert ! \
               gvadetect model=$DETECT_MODEL_PATH device=$DEVICE ! queue ! \
               gvawatermark ! videoconvert ! fpsdisplaysink video-sink=ximagesink sync=false
