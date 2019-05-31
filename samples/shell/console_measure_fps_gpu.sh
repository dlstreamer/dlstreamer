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

FILE=${1:-$VIDEO_EXAMPLES_DIR/Pexels_Videos_4786.mp4}

MODEL=vehicle-license-plate-detection-barrier-0106

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

export GST_VAAPI_ALL_DRIVERS=1

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} -v \
               filesrc location=${FILE} ! decodebin ! vaapipostproc ! video/x-raw,format=BGRA,width=300,height=300 ! \
               gvadetect model=$DETECT_MODEL_PATH device=GPU every-nth-frame=1 batch-size=1 ! \
               fpsdisplaysink video-sink=fakesink silent=true sync=false
