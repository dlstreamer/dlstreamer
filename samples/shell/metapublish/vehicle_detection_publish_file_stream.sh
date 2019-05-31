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

FILE=${1:-$VIDEO_EXAMPLES_DIR/Pexels_Videos_4786_960x540.mp4}
MODEL=vehicle-license-plate-detection-barrier-0106
PRECISION=${2:-\"FP32\"}
OUTFILE=${3:-"metapublish_report.json"}

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

# Note that two pipelines create instances of singleton element 'inf0', so we can specify parameters only in first instance
gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
                filesrc location=$FILE ! decodebin ! video/x-raw ! videoconvert ! \
                gvadetect inference-id=inf0 model=$DETECT_MODEL_PATH device=CPU every-nth-frame=1 batch-size=1 ! queue ! \
                gvawatermark ! videoconvert ! fakesink \
                filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! gvadetect inference-id=inf0 ! \
                gvametaconvert converter=json method=all ! \
                gvametapublish method=file filepath=${OUTFILE} outputformat=stream ! \
                queue ! gvawatermark ! videoconvert ! fakesink

