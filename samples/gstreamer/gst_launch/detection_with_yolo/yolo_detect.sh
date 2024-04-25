#!/bin/bash
# ==============================================================================
# Copyright (C) 2021-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
# This sample refers to a video file by Rihard-Clement-Ciprian Diac via Pexels
# (https://www.pexels.com)
# ==============================================================================

set -euo pipefail

MODEL=${1:-"yolox_s"} # Supported values: yolo_all, yolox-tiny, yolox_s, yolov7, yolov8s, yolov9c
DEVICE=${2:-"CPU"}    # Supported values: CPU, GPU, NPU
INPUT=${3:-"https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4"}
OUTPUT=${4:-"file"} # Supported values: file, display, fps, json, display-and-json

cd "$(dirname "$0")"

declare -A MODEL_PROC_FILES=(
  ["yolox-tiny"]="../../model_proc/public/yolo-x.json"
  ["yolox_s"]="../../model_proc/public/yolo-x.json"
  ["yolov5s"]="../../model_proc/public/yolo-v7.json"
  ["yolov5su"]="../../model_proc/public/yolo-v8.json"
  ["yolov7"]="../../model_proc/public/yolo-v7.json"
  ["yolov8s"]="../../model_proc/public/yolo-v8.json"
  ["yolov9c"]="../../model_proc/public/yolo-v8.json"
)

if ! [[ "${!MODEL_PROC_FILES[*]}" =~ $MODEL ]]; then
  echo "Unsupported model: $MODEL" >&2
  exit 1
fi

MODEL_PROC=$(realpath "${MODEL_PROC_FILES[$MODEL]}")

cd - 1>/dev/null

if [ -d "$PWD/public/$MODEL/FP16/$MODEL.xml" ]; then
  MODEL_PATH="$PWD/public/$MODEL/FP16/$MODEL.xml"
else
  MODEL_PATH="$PWD/public/$MODEL/FP32/$MODEL.xml"
fi

if [[ "$INPUT" == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ "$INPUT" == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

DECODE_ELEMENT=""
PREPROC_BACKEND="ie"
if [[ "$DEVICE" == "GPU" ]] || [[ "$DEVICE" == "NPU" ]]; then
  DECODE_ELEMENT="vaapipostproc ! video/x-raw(memory:VASurface) !"
  PREPROC_BACKEND="vaapi-surface-sharing"
fi

if [[ "$OUTPUT" == "file" ]]; then
  FILE=$(basename "${INPUT%.*}")
  rm -f "${FILE}_output.avi"
  SINK_ELEMENT="gvawatermark ! videoconvertscale ! gvafpscounter ! vaapih264enc ! avimux name=mux ! filesink location=${FILE}_output.avi"
elif [[ "$OUTPUT" == "display" ]] || [[ -z $OUTPUT ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvertscale ! gvafpscounter ! autovideosink sync=false"
elif [[ "$OUTPUT" == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false"
elif [[ "$OUTPUT" == "json" ]]; then
  rm -f output.json
  SINK_ELEMENT="gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines file-path=output.json ! fakesink async=false"
elif [[ "$OUTPUT" == "display-and-json" ]]; then
  rm -f output.json
  SINK_ELEMENT="gvawatermark ! gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines file-path=output.json ! videoconvert ! gvafpscounter ! autovideosink sync=false"
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Valid values: "file" - render to file, "display" - render to screen, "fps" - print FPS, "json" - write to output.json, "display-and-json" - render to screen and write to output.json
  exit
fi

PIPELINE="gst-launch-1.0 $SOURCE_ELEMENT ! decodebin ! $DECODE_ELEMENT \
gvadetect model=$MODEL_PATH"
if [[ -n "$MODEL_PROC" ]]; then
  PIPELINE="$PIPELINE model-proc=$MODEL_PROC"
fi
PIPELINE="$PIPELINE device=$DEVICE pre-process-backend=$PREPROC_BACKEND ! queue ! \
$SINK_ELEMENT"

echo "${PIPELINE}"
$PIPELINE
