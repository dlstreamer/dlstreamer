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

MODEL=ssd300

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

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL)

FILE=${1}

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} filesrc location=${FILE} ! \
               decodebin ! video/x-raw ! videoconvert ! \
               gvadetect model=$DETECT_MODEL_PATH ! queue ! \
               gvawatermark ! videoconvert ! fpsdisplaysink video-sink=glimagesink sync=false
