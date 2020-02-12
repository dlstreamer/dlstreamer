#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

BASEDIR=$(dirname "$0")/../../..
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh
#import GET_MODEL_PATH and PROC_PATH
source $BASEDIR/scripts/path_extractor.sh

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./face_recognition.sh <path/to/your/video/sample>"
  exit
fi

INPUT=${1}
GALLERY=${2:-"$GST_SAMPLES_DIR/shell/reidentification/gallery/gallery.json"}

DETECTION_MODEL=face-detection-adas-0001
LANDMARKS_MODEL=landmarks-regression-retail-0009
IDENTIFICATION_MODEL=face-reidentification-retail-0095

LANDMARKS_MODEL_PROC=landmarks-regression-retail-0009
IDENTIFICATION_MODEL_PROC=face-reidentification-retail-0095

DEVICE=CPU
PRE_PROC=opencv

DETECT_MODEL_PATH=$(GET_MODEL_PATH $DETECTION_MODEL )
LANDMARKS_MODEL_PATH=$(GET_MODEL_PATH $LANDMARKS_MODEL )
IDENTIFICATION_MODEL_PATH=$(GET_MODEL_PATH $IDENTIFICATION_MODEL )

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}
echo LD_LIBRARY_PATH=${LD_LIBRARY_PATH}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
  filesrc location=${INPUT} ! decodebin ! videoconvert ! video/x-raw,format=BGRx ! \
  gvadetect model=$DETECT_MODEL_PATH device=$DEVICE pre-proc=$PRE_PROC ! queue ! \
  gvaclassify model=$LANDMARKS_MODEL_PATH model-proc=$(PROC_PATH $LANDMARKS_MODEL_PROC) device=$DEVICE pre-proc=$PRE_PROC ! queue ! \
  gvaclassify model=$IDENTIFICATION_MODEL_PATH model-proc=$(PROC_PATH $IDENTIFICATION_MODEL_PROC) device=$DEVICE pre-proc=$PRE_PROC ! queue ! \
  gvaidentify gallery=${GALLERY} ! queue ! \
  gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
