#!/bin/bash
# ==============================================================================
# Copyright (C) 2021-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

INPUT=${1}
DEVICE=${2:-CPU}
SINK=${3:-display}

if [[ -z $INPUT ]]; then
  echo Error: Input video path is empty
  echo Please provide path to input video file
  exit
fi

if [[ $DEVICE == "CPU" ]]; then
  CONVERTER="videoconvert ! video/x-raw,format=BGRx"
  PREPROC_BACKEND="opencv"
elif [[ $DEVICE == "GPU" ]]; then
  CONVERTER="vaapipostproc ! video/x-raw\(memory:VASurface\)"
  PREPROC_BACKEND="vaapi-surface-sharing"
else
  echo Error wrong value for DEVICE parameter
  echo Possible values: CPU, GPU
  exit
fi

if [[ $SINK == "display" ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! gvafpscounter ! autovideosink sync=false"
elif [[ $SINK == "fps" ]]; then
  SINK_ELEMENT=" gvafpscounter ! fakesink async=false "
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Possible values: display - render, fps - show FPS only
  exit
fi

PROC_PATH() {
    echo $(dirname "$0")/model_proc/$1.json
}

MODEL_ENCODER=${MODELS_PATH}/intel/action-recognition-0001/action-recognition-0001-encoder/FP32/action-recognition-0001-encoder.xml
MODEL_DECODER=${MODELS_PATH}/intel/action-recognition-0001/action-recognition-0001-decoder/FP32/action-recognition-0001-decoder.xml
MODEL_PROC=$(PROC_PATH action-recognition-0001)

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

# Pipeline uses gvaactionrecognitionbin

PIPELINE="gst-launch-1.0 \
$SOURCE_ELEMENT ! \
decodebin ! \
$CONVERTER ! \
gvaactionrecognitionbin pre-process-backend=$PREPROC_BACKEND \
model-proc=$MODEL_PROC \
enc-device=$DEVICE \
enc-model=$MODEL_ENCODER \
dec-device=$DEVICE \
dec-model=$MODEL_DECODER \
postproc-converter=to_label threshold=0.5 ! \
$SINK_ELEMENT"

echo ${PIPELINE}
eval $PIPELINE
