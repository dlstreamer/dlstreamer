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

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./console_measure_fps_cpu.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

MODEL=vehicle-license-plate-detection-barrier-0106

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

unset GST_VAAPI_ALL_DRIVERS

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} -v \
               filesrc location=${FILE} ! decodebin ! videoconvert ! \
               videoscale ! video/x-raw,format=BGRA,width=300,height=300 ! \
               gvadetect model=$DETECT_MODEL_PATH device=CPU every-nth-frame=1 batch-size=1 ! \
               fpsdisplaysink video-sink=fakesink silent=true sync=false
