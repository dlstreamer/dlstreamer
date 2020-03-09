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

MODEL=mobilenet-ssd

DEVICE=CPU
PRE_PROC=ie

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./object_detection_and_transcode.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
               filesrc location=$FILE ! decodebin ! video/x-raw ! videoconvert ! \
               gvadetect model=$DETECT_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC every-nth-frame=1 batch-size=1 ! queue ! \
               gvawatermark ! videoconvert ! vaapih264enc ! filesink location=out.h264
