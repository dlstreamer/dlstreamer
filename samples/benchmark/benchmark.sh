#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage : ./benchmark.sh VIDEO_FILE [DECODE_DEVICE] [INFERENCE_DEVICE] [CHANNELS_COUNT]"
  echo "You can download video with"
  echo "\"curl https://raw.githubusercontent.com/intel-iot-devkit/sample-videos/master/head-pose-face-detection-female-and-male.mp4 --output /path/to/your/video/head-pose-face-detection-female-and-male.mp4\""
  echo "and run sample ./benchmark.sh /path/to/your/video/head-pose-face-detection-female-and-male.mp4"
  exit
fi

VIDEO_FILE_NAME=${1}
DECODE_DEVICE=${2:-CPU}
INFERENCE_DEVICE=${3:-CPU}
CHANNELS_COUNT=${4:-1}

MODEL=face-detection-adas-0001

PROC_PATH() {
    echo ./model_proc/$1.json
}

DETECT_MODEL_PATH=${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml

if [ $DECODE_DEVICE == CPU ]; then
  DECODER="decodebin force-sw-decoders=true ! video/x-raw"
else
  DECODER="decodebin ! video/x-raw\(memory:DMABuf\)"
fi


PIPELINE=" filesrc location=${VIDEO_FILE_NAME} ! \
${DECODER} ! \
gvadetect model-instance-id=inf0 model=${DETECT_MODEL_PATH} device=${INFERENCE_DEVICE} ! queue ! \
gvafpscounter ! fakesink async=false "

FINAL_PIPELINE_STR=""

for (( CURRENT_CHANNELS_COUNT=0; CURRENT_CHANNELS_COUNT < $CHANNELS_COUNT; ++CURRENT_CHANNELS_COUNT ))
do
  FINAL_PIPELINE_STR+=$PIPELINE
done


echo "gst-launch-1.0 -v ${FINAL_PIPELINE_STR}"
eval gst-launch-1.0 -v ${FINAL_PIPELINE_STR}
