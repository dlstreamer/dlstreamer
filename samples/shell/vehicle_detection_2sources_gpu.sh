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

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./vehicle_detection_2sources_gpu.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

MODEL=vehicle-license-plate-detection-barrier-0106

PRE_PROC=ie

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

# Note that two pipelines create instances of singleton element 'inf0', so we can specify parameters only in first instance
gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
               filesrc location=$FILE ! decodebin ! video/x-raw ! videoconvert ! \
               gvadetect inference-id=inf0 model=$DETECT_MODEL_PATH device=GPU pre-proc=$PRE_PROC every-nth-frame=1 batch-size=1 ! queue ! \
               gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false \
               filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! \
               gvadetect inference-id=inf0 ! queue ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
