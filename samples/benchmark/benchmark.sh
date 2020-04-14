#!/bin/bash
# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage : ./benchmark.sh VIDEO_FILE [DECODE_DEVICE] [INFERENCE_DEVICE] [CHANNELS_COUNT]"
  echo "You can download video with \"cd /path/to/your/video/ && wget https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4\""
  echo " and run sample ./benchmark.sh /path/to/your/video/head-pose-face-detection-female-and-male.mp4"
  exit
fi

SOURCE_ELEMENT="filesrc location=${1}"
DECODE_DEVICE=${2:-CPU}
INFERENCE_DEVICE=${3:-CPU}
CHANNELS_COUNT=${4:-1}

MODEL=face-detection-adas-0001

GET_MODEL_PATH() {
    model_name=$1
    precision="FP32"
    for models_dir in ${MODELS_PATH//:/ }; do
        paths=$(find $models_dir -type f -name "*$model_name.xml" -print)
        if [ ! -z "$paths" ];
        then
            considered_precision_paths=$(echo "$paths" | grep "/$precision/")
           if [ ! -z "$considered_precision_paths" ];
            then
                echo $(echo "$considered_precision_paths" | head -n 1)
                exit 0
            else
                echo $(echo "$paths" | head -n 1)
                exit 0
            fi
        fi
    done

    echo -e "\e[31mModel $model_name file was not found. Please set MODELS_PATH\e[0m" 1>&2
    exit 1
}

PROC_PATH() {
    echo ./model_proc/$1.json
}

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

if [ $DECODE_DEVICE == CPU ]; then
  unset GST_VAAPI_ALL_DRIVERS
  VIDEO_PROCESSING="decodebin ! videoscale ! video/x-raw"
  PRE_PROC=ie
else
  export GST_VAAPI_ALL_DRIVERS=1
  VIDEO_PROCESSING="decodebin ! vaapipostproc ! video/x-raw(memory:VASurface)"
  PRE_PROC=vaapi
fi

PIPELINE=" ${SOURCE_ELEMENT} ! ${VIDEO_PROCESSING} ! \
gvadetect model-instance-id=inf0 model=${DETECT_MODEL_PATH} device=${INFERENCE_DEVICE} pre-process-backend=${PRE_PROC} ! queue ! \
gvafpscounter ! fakesink "

FINAL_PIPELINE_STR=""

for (( CURRENT_CHANNELS_COUNT=0; CURRENT_CHANNELS_COUNT < $CHANNELS_COUNT; ++CURRENT_CHANNELS_COUNT ))
do
  FINAL_PIPELINE_STR+=$PIPELINE
done


echo "gst-launch-1.0 -v ${FINAL_PIPELINE_STR}"
gst-launch-1.0 -v ${FINAL_PIPELINE_STR}
