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

MODEL=vehicle-license-plate-detection-barrier-0106

DEVICE=GPU

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./vehicle_license_plate_detection_barrier_0106.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} filesrc location=${FILE} ! \
               decodebin ! video/x-raw ! videoconvert ! \
               gvadetect gpu-streams=1 model=$DETECT_MODEL_PATH device=$DEVICE ! queue ! \
               gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
