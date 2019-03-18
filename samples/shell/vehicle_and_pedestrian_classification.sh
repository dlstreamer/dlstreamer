#!/bin/bash
# ==============================================================================
# Copyright (C) <2018-2019> Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

if [ -z "$1" ]
then
    echo "Please provide path to video file as first argument"
    exit 1
fi

BASEDIR=$(dirname "$0")/../..
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi

FILE=${1}

MODEL1=person-vehicle-bike-detection-crossroad-0078
MODEL2=person-attributes-recognition-crossroad-0200
MODEL3=vehicle-attributes-recognition-barrier-0039

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

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL1)
CLASS_MODEL_PATH=$(GET_MODEL_PATH $MODEL2)
CLASS_MODEL_PATH1=$(GET_MODEL_PATH $MODEL3)

PROC_PATH() {
  echo ${GST_SAMPLES_DIR}/model_proc/$1.json
}

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}
echo LD_LIBRARY_PATH=${LD_LIBRARY_PATH}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
  filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! \
  gvadetect   model=$DETECT_MODEL_PATH model-proc=$(PROC_PATH $MODEL1) ! queue ! \
  gvaclassify model=$CLASS_MODEL_PATH model-proc=$(PROC_PATH $MODEL2) object-class=person ! queue ! \
  gvaclassify model=$CLASS_MODEL_PATH1 model-proc=$(PROC_PATH $MODEL3) object-class=vehicle ! queue ! \
  gvawatermark ! videoconvert ! fpsdisplaysink video-sink=glimagesink sync=false
