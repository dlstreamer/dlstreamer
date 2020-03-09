#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e
SCRIPT=$(realpath $0)
SCRIPTDIR=$(dirname $SCRIPT)
BASEDIR=$(dirname $SCRIPT)/../../..
echo $BASEDIR
if [ -n ${GST_SAMPLES_DIR} ]; then
  source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh
source $BASEDIR/scripts/path_extractor.sh

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./bottle_count.sh <path/to/your/video/sample>"
  exit
fi

INPUT=${1}

MODEL=mobilenet-ssd
MODEL_PROC=mobilenet-ssd

PYTHON_SCRIPT=$SCRIPTDIR/../postproc_callbacks/bottle_count.py

DEVICE=CPU
PRE_PROC=ie
DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL)
MODEL_PROC_PATH=$SCRIPTDIR/../../model_proc/$MODEL.json

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == "rtsp://"* ]]; then
  SOURCE_ELEMENT="urisourcebin uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}
echo LD_LIBRARY_PATH=${LD_LIBRARY_PATH}

echo gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
$SOURCE_ELEMENT ! qtdemux ! avdec_h264 ! video/x-raw ! videoconvert ! \
gvadetect model=$DETECT_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC model-proc=${MODEL_PROC_PATH} ! queue ! \
gvapython module=$PYTHON_SCRIPT class=BottleCount ! \
gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false


gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
$SOURCE_ELEMENT ! qtdemux ! avdec_h264 ! videoconvert ! video/x-raw,format=BGRx ! \
gvadetect model=$DETECT_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC model-proc=${MODEL_PROC_PATH} ! queue ! \
gvapython module=$PYTHON_SCRIPT class=BottleCount ! \
gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
