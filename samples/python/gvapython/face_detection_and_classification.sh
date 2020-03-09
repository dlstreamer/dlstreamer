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
  echo "Usage: ./face_detection_and_classification.sh <path/to/your/video/sample>"
  exit
fi

INPUT=${1}

MODEL1=face-detection-adas-0001
MODEL2=age-gender-recognition-retail-0013

PYTHON_SCRIPT1=$SCRIPTDIR/../postproc_callbacks/ssd_object_detection.py
PYTHON_SCRIPT2=$SCRIPTDIR/../postproc_callbacks/age_gender_classification.py

DEVICE=CPU
PRE_PROC=ie

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL1)
CLASS_MODEL_PATH=$(GET_MODEL_PATH $MODEL2)

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

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
$SOURCE_ELEMENT ! decodebin ! videoconvert ! video/x-raw,format=BGRx ! \
gvainference model=$DETECT_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC ! queue ! \
gvapython module=$PYTHON_SCRIPT1 ! \
gvaclassify model=$CLASS_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC ! queue ! \
gvapython module=$PYTHON_SCRIPT2 ! \
 gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
