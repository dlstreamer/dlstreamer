#!/bin/bash
# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

BASEDIR=$(dirname "$0")/../../..
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi

INPUT=${1:-$PWD/classroom.mp4}
GALLERY=$GST_SAMPLES_DIR/shell/reidentification/gallery/gallery.json

DETECTION_MODEL=face-detection-adas-0001
LANDMARKS_MODEL=landmarks-regression-retail-0009
IDENTIFICATION_MODEL=face-reidentification-retail-0095

PRECISION=${2:-\"FP32\"}

GET_MODEL_PATH() {
    for path in ${MODELS_PATH//:/ }; do
        paths=$(find $path -name "$1.xml" -print)
        if [ ! -z "$paths" ];
        then
            echo $(grep -l "precision=$PRECISION" $paths)
            exit 0
        fi
    done
    echo -e "\e[31mModel $1.xml file was not found. Please set MODELS_PATH\e[0m" 1>&2
    exit 1
}

DETECT_MODEL_PATH=$(GET_MODEL_PATH $DETECTION_MODEL)
LANDMARKS_MODEL_PATH=$(GET_MODEL_PATH $LANDMARKS_MODEL)
IDENTIFICATION_MODEL_PATH=$(GET_MODEL_PATH $IDENTIFICATION_MODEL)

PROC_PATH() {
    echo ${GST_SAMPLES_DIR}/model_proc/$1.json
}

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}
echo LD_LIBRARY_PATH=${LD_LIBRARY_PATH}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
  filesrc location=${INPUT} ! decodebin ! video/x-raw ! videoconvert ! \
  gvadetect   model=$DETECT_MODEL_PATH ! queue ! \
  gvaclassify model=$LANDMARKS_MODEL_PATH model-proc=$(PROC_PATH $LANDMARKS_MODEL) ! queue ! \
  gvaclassify use-landmarks=false model=$IDENTIFICATION_MODEL_PATH model-proc=$(PROC_PATH $IDENTIFICATION_MODEL) ! queue ! \
  gvaidentify gallery=${GALLERY} ! queue ! \
  gvawatermark ! \
  videoconvert ! fpsdisplaysink video-sink=glimagesink sync=false
