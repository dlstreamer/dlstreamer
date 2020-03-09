#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
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

unset GST_VAAPI_ALL_DRIVERS

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./face_detection_multichannel.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}
CHANNELS_COUNT=${2:-2}
DEVICE=CPU
PRE_PROC=ie

MODEL=face-detection-retail-0004

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

PIPELINE_STR=" filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! \
             gvadetect inference-id=inf0 model=$DETECT_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC batch-size=1 nireq=5 ! queue ! \
             gvawatermark ! \
             videoconvert ! fpsdisplaysink video-sink=fakesink sync=false async-handling=true"

FINAL_PIPELINE_STR=""

for (( CURRENT_CHANNELS_COUNT=0; CURRENT_CHANNELS_COUNT < $CHANNELS_COUNT; ++CURRENT_CHANNELS_COUNT ))
do
  FINAL_PIPELINE_STR+=$PIPELINE_STR
done

gst-launch-1.0 -v --gst-plugin-path ${GST_PLUGIN_PATH} ${FINAL_PIPELINE_STR}
