#!/bin/bash
# ==============================================================================
# Copyright (C) 2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
# This sample refers to a video file by Rihard-Clement-Ciprian Diac via Pexels
# (https://www.pexels.com)
# ==============================================================================

set -euo pipefail

if [ -z "${MODELS_PATH:-}" ]; then
  echo "Error: MODELS_PATH is not set." >&2 
  exit 1
else 
  echo "MODELS_PATH: $MODELS_PATH"
fi

INPUT=${1:-"https://videos.pexels.com/video-files/1192116/1192116-sd_640_360_30fps.mp4"}
DEVICE_STREAM_12=${2:-"NPU"}    # Supported values: CPU, GPU, NPU
DEVICE_STREAM_34=${3:-"GPU"}    # Supported values: CPU, GPU, NPU
GSTVA=${4:-"VA"}                # Supported values: VA, VAAPI
OUTPUT=${5:-"file"}             # Supported values: file, json

cd "$(dirname "$0")"

MODEL_PATH="${MODELS_PATH}/public/yolov8s/FP16/yolov8s.xml"

# check if model exists in local directory
if [ ! -f $MODEL_PATH ]; then
  echo "Model not found: ${MODEL_PATH}"
  exit 
fi

MODEL_PROC_RELATIVE="../../model_proc/public/yolo-v8.json"
MODEL_PROC=$(realpath "${MODEL_PROC_RELATIVE}")

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

if [[ "$GSTVA" != "VA" ]] && [[ "$GSTVA" != "VAAPI" ]]; then 
  echo "Error: Wrong value for GSTVA parameter."
  echo "Valid values: VA, VAAPI"
  exit
fi

### STREAM 1 and 2 ###
# CPU device
DECODE_ELEMENT_STR12="decodebin"
PREPROC_BACKEND_STR12="ie"
# GPU , NPU device
## GST-VA ##
if [[ "$GSTVA" == "VA" ]]; then 
  if [[ "$DEVICE_STREAM_12" == "GPU" ]]; then
    PREPROC_BACKEND_STR12="va-surface-sharing"
  fi
  if [[ "$DEVICE_STREAM_12" == "GPU" ]] || [[ "$DEVICE_STREAM_12" == "NPU" ]]; then
    DECODE_ELEMENT_STR12="qtdemux ! vah264dec"
    DECODE_ELEMENT_STR12+=" ! vapostproc ! video/x-raw(memory:VAMemory)"
    PREPROC_BACKEND_STR12="${PREPROC_BACKEND_STR12} nireq=4 model-instance-id=inf0"
  fi
fi
## GST-VAAPI ##
if [[ "$GSTVA" == "VAAPI" ]]; then 
  if [[ "$DEVICE_STREAM_12" == "GPU" ]]; then
    PREPROC_BACKEND_STR12="vaapi-surface-sharing"
  fi
  if [[ "$DEVICE_STREAM_12" == "GPU" ]] || [[ "$DEVICE_STREAM_12" == "NPU" ]]; then
    DECODE_ELEMENT_STR12+="! vaapipostproc ! video/x-raw(memory:VASurface)"
    PREPROC_BACKEND_STR12="${PREPROC_BACKEND_STR12} nireq=4 model-instance-id=inf0"
  fi
fi

### STREAM 3 and 4 ###
# CPU device
DECODE_ELEMENT_STR34="decodebin"
PREPROC_BACKEND_STR34="ie"
# GPU , NPU device
## GST-VA ##
if [[ "$GSTVA" == "VA" ]]; then 
  if [[ "$DEVICE_STREAM_34" == "GPU" ]]; then
    PREPROC_BACKEND_STR34="va-surface-sharing"
  fi
  if [[ "$DEVICE_STREAM_34" == "GPU" ]] || [[ "$DEVICE_STREAM_34" == "NPU" ]]; then
    DECODE_ELEMENT_STR34="qtdemux ! vah264dec"
    DECODE_ELEMENT_STR34+="! vapostproc ! video/x-raw(memory:VAMemory)"
    PREPROC_BACKEND_STR34="${PREPROC_BACKEND_STR34} nireq=4 model-instance-id=inf1"
  fi
fi
## GST-VAAPI ##
if [[ "$GSTVA" == "VAAPI" ]]; then 
  if [[ "$DEVICE_STREAM_34" == "GPU" ]]; then
    PREPROC_BACKEND_STR34="vaapi-surface-sharing"
  fi
  if [[ "$DEVICE_STREAM_34" == "GPU" ]] || [[ "$DEVICE_STREAM_34" == "NPU" ]]; then
    DECODE_ELEMENT_STR34+="! vaapipostproc ! video/x-raw(memory:VASurface)"
    PREPROC_BACKEND_STR34="${PREPROC_BACKEND_STR34} nireq=4 model-instance-id=inf1"
  fi
fi

if [[ "$GSTVA" == "VA" ]]; then
  SINK_ELEMENT_BASE="gvawatermark ! videoconvertscale ! gvafpscounter ! vah264enc ! h264parse !"
fi
if [[ "$GSTVA" == "VAAPI" ]]; then 
  SINK_ELEMENT_BASE="gvawatermark ! videoconvertscale ! gvafpscounter ! vaapih264enc ! h264parse !"
fi

if [[ "$OUTPUT" == "file" ]]; then
  FILE=$(basename "${INPUT%*.*}")
  rm -f ${FILE}_*.mp4
  SINK_ELEMENT_STR_1="${SINK_ELEMENT_BASE} filesink location=${FILE}_${GSTVA}_${DEVICE_STREAM_12}_1.mp4"
  SINK_ELEMENT_STR_2="${SINK_ELEMENT_BASE} filesink location=${FILE}_${GSTVA}_${DEVICE_STREAM_12}_2.mp4"
  SINK_ELEMENT_STR_3="${SINK_ELEMENT_BASE} filesink location=${FILE}_${GSTVA}_${DEVICE_STREAM_34}_3.mp4"
  SINK_ELEMENT_STR_4="${SINK_ELEMENT_BASE} filesink location=${FILE}_${GSTVA}_${DEVICE_STREAM_34}_4.mp4"
elif [[ "$OUTPUT" == "json" ]]; then
  rm -f output.json
  SINK_ELEMENT_BASE="gvametaconvert add-tensor-data=true ! gvametapublish file-format=json-lines"
  SINK_ELEMENT_STR_1="${SINK_ELEMENT_BASE} file-path=output_${GSTVA}_${DEVICE_STREAM_12}_1.json ! fakesink async=false"
  SINK_ELEMENT_STR_2="${SINK_ELEMENT_BASE} file-path=output_${GSTVA}_${DEVICE_STREAM_12}_2.json ! fakesink async=false"
  SINK_ELEMENT_STR_3="${SINK_ELEMENT_BASE} file-path=output_${GSTVA}_${DEVICE_STREAM_34}_3.json ! fakesink async=false"
  SINK_ELEMENT_STR_4="${SINK_ELEMENT_BASE} file-path=output_${GSTVA}_${DEVICE_STREAM_34}_4.json ! fakesink async=false"
else
  echo Error: Wrong value for SINK_ELEMENT parameter
  echo Valid values: "file" - render to file, "json" - write to output.json
  exit
fi

PIPELINE_STREAM_1="${SOURCE_ELEMENT} ! ${DECODE_ELEMENT_STR12} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_12} pre-process-backend=${PREPROC_BACKEND_STR12} ! queue ! ${SINK_ELEMENT_STR_1}"

PIPELINE_STREAM_2="${SOURCE_ELEMENT} ! ${DECODE_ELEMENT_STR12} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_12} pre-process-backend=${PREPROC_BACKEND_STR12} ! queue ! ${SINK_ELEMENT_STR_2}"

PIPELINE_STREAM_3="${SOURCE_ELEMENT} ! ${DECODE_ELEMENT_STR34} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_34} pre-process-backend=${PREPROC_BACKEND_STR34} ! queue ! ${SINK_ELEMENT_STR_3}"

PIPELINE_STREAM_4="${SOURCE_ELEMENT} ! ${DECODE_ELEMENT_STR34} ! gvadetect ${MODEL_PATH_PROC} \
device=${DEVICE_STREAM_34} pre-process-backend=${PREPROC_BACKEND_STR34} ! queue ! ${SINK_ELEMENT_STR_4}"

PIPELINE="gst-launch-1.0 \
${PIPELINE_STREAM_1} \
${PIPELINE_STREAM_2} \
${PIPELINE_STREAM_3} \
${PIPELINE_STREAM_4}"

echo "${PIPELINE}"

${PIPELINE}

