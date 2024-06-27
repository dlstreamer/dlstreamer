#!/bin/bash
# ==============================================================================
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

if [ -z "${MODELS_PATH:-}" ]; then
  echo "Error: MODELS_PATH is not set." >&2 
  exit 1
else 
  echo "MODELS_PATH: $MODELS_PATH"
fi

INPUT=${1}
DEVICE=${2:-CPU}
OUTPUT=${3:-display} # Supported values: display, fps, json, display-and-json

MODEL_ENCODER=${MODELS_PATH}/intel/action-recognition-0001/action-recognition-0001-encoder/FP32/action-recognition-0001-encoder.xml
MODEL_DECODER=${MODELS_PATH}/intel/action-recognition-0001/action-recognition-0001-decoder/FP32/action-recognition-0001-decoder.xml

if [[ -z $INPUT ]]; then
  echo Error: Input video path is empty
  echo Please provide path to input video file
  exit
fi

if [[ $OUTPUT == "display" ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! gvafpscounter ! autovideosink sync=false"
elif [[ $OUTPUT == "fps" ]]; then
  SINK_ELEMENT=" gvafpscounter ! fakesink async=false"
elif [[ $OUTPUT == "json" ]]; then
  rm -f output.json
  SINK_ELEMENT=" gvametaconvert ! gvametapublish file-format=json-lines file-path=output.json ! fakesink async=false"
elif [[ $OUTPUT == "display-and-json" ]]; then
  rm -f output.json
  SINK_ELEMENT="gvawatermark ! gvametaconvert ! gvametapublish file-format=json-lines file-path=output.json ! videoconvert ! gvafpscounter ! autovideosink sync=false"
else
  echo Error wrong value for OUTPUT parameter
  echo Valid values: "display" - render to screen, "fps" - print FPS, "json" - write to output.json, "display-and-json" - render to screen and write to output.json
  exit
fi

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

DIR=$(dirname "$0")

PIPELINE="gst-launch-1.0 \
$SOURCE_ELEMENT ! \
decodebin ! \
video_inference \
  process=' \
    openvino_tensor_inference model=$MODEL_ENCODER device=$DEVICE ! \
    tensor_sliding_window ! \
    openvino_tensor_inference model=$MODEL_DECODER device=$DEVICE' \
  postprocess=' \
    tensor_postproc_label labels-file=$DIR/kinetics_400.txt method=softmax' ! \
$SINK_ELEMENT"

echo "${PIPELINE}"
eval "$PIPELINE"
