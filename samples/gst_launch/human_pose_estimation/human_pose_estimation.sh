#!/bin/bash
# ==============================================================================
# Copyright (C) 2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/face-demographics-walking.mp4}

DEVICE=${2:-CPU}

if [[ $3 == "display" ]] || [[ -z $3 ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false"
elif [[ $3 == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false "
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Possible values: display - render, fps - show FPS only
  exit
fi

HPE_MODEL=human-pose-estimation-0001


if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

PROC_PATH() {
    echo $(dirname "$0")/model_proc/$1.json
}

HPE_MODEL_PATH=${MODELS_PATH}/intel/${HPE_MODEL}/FP32/${HPE_MODEL}.xml
HPE_MODEL_PROC=$(PROC_PATH $HPE_MODEL)

PIPELINE="gst-launch-1.0 $SOURCE_ELEMENT ! decodebin ! \
gvaclassify model=$HPE_MODEL_PATH model-proc=$HPE_MODEL_PROC device=$DEVICE inference-region=full-frame ! queue ! \
$SINK_ELEMENT"

echo ${PIPELINE}
${PIPELINE}
