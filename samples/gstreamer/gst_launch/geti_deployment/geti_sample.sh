#!/bin/bash
# ==============================================================================
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
# This sample refers to a video file by Rihard-Clement-Ciprian Diac via Pexels
# (https://www.pexels.com)
# ==============================================================================
set -e

if [ -z "${MODELS_PATH:-}" ]; then
  echo "Error: MODELS_PATH is not set." >&2 
  exit 1
else 
  echo "MODELS_PATH: $MODELS_PATH"
fi

MODEL=${1:-detection} # Supported values: detection, classification_single, classification_multi
DEVICE=${2:-GPU} # Supported values: CPU, GPU, NPU
INPUT=${3:-https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4}
OUTPUT=${4:-file} # Supported values: file, display, fps, json, display-and-json

MODEL_PATH="${MODELS_PATH}/intel/$MODEL/FP16/$MODEL.xml"

# check if model exists in local directory
if [ ! -f $MODEL_PATH ]; then
  echo "Model not found: ${MODEL_PATH}"
  exit 
fi

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

if [[ $DEVICE == "CPU" ]]; then
  DECODE_ELEMENT=" ! decodebin !"
  PREPROC_BACKEND="ie"
elif [[ $DEVICE == "GPU" ]] || [[ $DEVICE == "NPU" ]]; then
  DECODE_ELEMENT="! qtdemux ! vah264dec"
  DECODE_ELEMENT+=" ! video/x-raw\(memory:VAMemory\)"
  PREPROC_BACKEND="va-surface-sharing"
fi

INFERENCE_ELEMENT="gvadetect"
if [[ $MODEL == "classification_single" ]] || [[ $MODEL == "classification_multi" ]]; then
  INFERENCE_ELEMENT="gvaclassify inference-region=full-frame"
fi

if [[ $OUTPUT == "file" ]]; then
  FILE="$(basename ${INPUT%.*})"
  rm -f ${FILE}_output.avi
  SINK_ELEMENT="gvawatermark ! videoconvertscale ! gvafpscounter ! vah264enc ! avimux name=mux ! filesink location=${FILE}_${MODEL}.avi"
elif [[ $OUTPUT == "display" ]] || [[ -z $OUTPUT ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvertscale ! gvafpscounter ! autovideosink sync=false"
elif [[ $OUTPUT == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false"
elif [[ $OUTPUT == "json" ]]; then
  rm -f output.json
  SINK_ELEMENT="gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines file-path=output.json ! fakesink async=false"
elif [[ $OUTPUT == "display-and-json" ]]; then
  rm -f output.json
  SINK_ELEMENT="gvawatermark ! gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines file-path=output.json ! videoconvert ! gvafpscounter ! autovideosink sync=false"
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Valid values: "file" - render to file, "display" - render to screen, "fps" - print FPS, "json" - write to output.json, "display-and-json" - render to screen and write to output.json
  exit
fi

PIPELINE="gst-launch-1.0 $SOURCE_ELEMENT $DECODE_ELEMENT \
$INFERENCE_ELEMENT model=$MODEL_PATH device=$DEVICE pre-process-backend=$PREPROC_BACKEND ! queue ! \
$SINK_ELEMENT"

echo "${PIPELINE}"
$PIPELINE
