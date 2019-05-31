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

FILE=${1:-$VIDEO_EXAMPLES_DIR/Fun_at_a_Fair.mp4}

MODEL=face-detection-retail-0004

DEVICE=CPU

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

PIPELINE_STR="filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! \
             gvadetect inference-id=inf0 model=$DETECT_MODEL_PATH device=$DEVICE batch-size=1 nireq=5 ! queue ! \
             gvawatermark name=gvawatermark ! \
             videoconvert ! fpsdisplaysink video-sink=fakesink sync=false async-handling=true"

channels_count=${3:-2}
current_channels_count=1

while [ $current_channels_count -lt $channels_count ]
do
  PIPELINE_STR+=" filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! \
                  gvadetect inference-id=inf0 model=$DETECT_MODEL_PATH device=$DEVICE batch-size=1 nireq=5 ! queue ! \
                  gvawatermark ! \
                  videoconvert ! fpsdisplaysink video-sink=fakesink sync=false async-handling=true"
  let current_channels_count=current_channels_count+1
done

unset GST_VAAPI_ALL_DRIVERS

gst-launch-1.0 -v --gst-plugin-path ${GST_PLUGIN_PATH} ${PIPELINE_STR}
