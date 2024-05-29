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

INPUT=${1:-"https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4"}
DEVICE_STREAM_12=${2:-"NPU"}    # Supported values: CPU, GPU, NPU
DEVICE_STREAM_34=${3:-"GPU"}    # Supported values: CPU, GPU, NPU

cd "$(dirname "$0")"

MODEL_PROC="../../model_proc/public/yolo-v8.json"
MODEL_PATH="$PWD/public/yolov8s/FP16/yolov8s.xml"
MODEL_PATH_PROC="model=${MODEL_PATH} model-proc=${MODEL_PROC}"

cd - 1>/dev/null

if [ ! -f ${MODEL_PATH} ]; then
  echo "Error: Model file does not exits ${MODEL_PATH}"
  exit
fi

if [ ! -f ${INPUT} ]; then
  echo "Error: Input file does not exits ${PWD}/${INPUT}"
  exit
fi

SOURCE_ELEMENT="filesrc location=${INPUT}"

DECODE_ELEMENT_STR12=""
PREPROC_BACKEND_STR12="ie"
if [[ "$DEVICE_STREAM_12" == "GPU" ]]; then
  PREPROC_BACKEND_STR12="vaapi-surface-sharing"
fi
if [[ "$DEVICE_STREAM_12" == "GPU" ]] || [[ "$DEVICE_STREAM_12" == "NPU" ]]; then
  DECODE_ELEMENT_STR12="! vaapipostproc ! video/x-raw(memory:VASurface)"
  PREPROC_BACKEND_STR12="${PREPROC_BACKEND_STR12} nireq=4 model-instance-id=inf0"
fi

DECODE_ELEMENT_STR34=""
PREPROC_BACKEND_STR34="ie"
if [[ "$DEVICE_STREAM_34" == "GPU" ]]; then
  PREPROC_BACKEND_STR34="vaapi-surface-sharing"
fi
if [[ "$DEVICE_STREAM_34" == "GPU" ]] || [[ "$DEVICE_STREAM_34" == "NPU" ]]; then
  DECODE_ELEMENT_STR34="vaapipostproc ! video/x-raw(memory:VASurface)"
  PREPROC_BACKEND_STR34="${PREPROC_BACKEND_STR34} nireq=4 model-instance-id=inf1"
fi

FILE=$(basename "${INPUT%*.*}")
rm -f ${FILE}_*.mp4

SINK_ELEMENT_BASE="gvawatermark ! videoconvertscale ! gvafpscounter ! vaapih264enc ! h264parse ! mp4mux !"
SINK_ELEMENT_STR_1="${SINK_ELEMENT_BASE} filesink location=${FILE}_${DEVICE_STREAM_12}_1.mp4"
SINK_ELEMENT_STR_2="${SINK_ELEMENT_BASE} filesink location=${FILE}_${DEVICE_STREAM_12}_2.mp4"
SINK_ELEMENT_STR_3="${SINK_ELEMENT_BASE} filesink location=${FILE}_${DEVICE_STREAM_34}_3.mp4"
SINK_ELEMENT_STR_4="${SINK_ELEMENT_BASE} filesink location=${FILE}_${DEVICE_STREAM_34}_4.mp4"

PIPELINE_STREAM_1="${SOURCE_ELEMENT} ! decodebin ${DECODE_ELEMENT_STR12} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_12} pre-process-backend=${PREPROC_BACKEND_STR12} ! queue ! ${SINK_ELEMENT_STR_1}"

PIPELINE_STREAM_2="${SOURCE_ELEMENT} ! decodebin ${DECODE_ELEMENT_STR12} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_12} pre-process-backend=${PREPROC_BACKEND_STR12} ! queue ! ${SINK_ELEMENT_STR_2}"

PIPELINE_STREAM_3="${SOURCE_ELEMENT} ! decodebin ${DECODE_ELEMENT_STR34} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_34} pre-process-backend=${PREPROC_BACKEND_STR34} ! queue ! ${SINK_ELEMENT_STR_3}"

PIPELINE_STREAM_4="${SOURCE_ELEMENT} ! decodebin ${DECODE_ELEMENT_STR34} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_34} pre-process-backend=${PREPROC_BACKEND_STR34} ! queue ! ${SINK_ELEMENT_STR_4}"

PIPELINE="gst-launch-1.0 \
${PIPELINE_STREAM_1} \
${PIPELINE_STREAM_2} \
${PIPELINE_STREAM_3} \
${PIPELINE_STREAM_4}"

echo "${PIPELINE}"

${PIPELINE}