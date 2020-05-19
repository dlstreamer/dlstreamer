#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}

MODEL1=face-detection-adas-0001
MODEL2=age-gender-recognition-retail-0013

DEVICE=CPU

SCRIPTDIR="$(dirname "$(realpath "$0")")"
PYTHON_SCRIPT1=$SCRIPTDIR/postproc_callbacks/ssd_object_detection.py
PYTHON_SCRIPT2=$SCRIPTDIR/postproc_callbacks/age_gender_classification.py

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

GET_MODEL_PATH() {
    model_name=$1
    precision=${2:-"FP32"}
    for models_dir in ${MODELS_PATH//:/ }; do
        paths=$(find $models_dir -type f -name "*$model_name.xml" -print)
        if [ ! -z "$paths" ];
        then
            considered_precision_paths=$(echo "$paths" | grep "/$precision/")
            if [ ! -z "$considered_precision_paths" ];
            then
                echo $(echo "$considered_precision_paths" | head -n 1)
                exit 0
            fi
        fi
    done

    echo -e "\e[31mModel $model_name file was not found. Please set MODELS_PATH\e[0m" 1>&2
    exit 1
}

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL1)
CLASS_MODEL_PATH=$(GET_MODEL_PATH $MODEL2)

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}

PIPELINE="gst-launch-1.0 \
$SOURCE_ELEMENT ! decodebin ! videoconvert ! video/x-raw,format=BGRx ! \
gvainference model=$DETECT_MODEL_PATH device=$DEVICE ! queue ! \
gvapython module=$PYTHON_SCRIPT1 ! \
gvaclassify model=$CLASS_MODEL_PATH device=$DEVICE ! queue ! \
gvapython module=$PYTHON_SCRIPT2 ! \
gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false"

echo ${PIPELINE}
PYTHONPATH=$PYTHONPATH:$(dirname "$0")/../../../../python \
${PIPELINE}
