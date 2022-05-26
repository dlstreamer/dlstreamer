#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}

DEVICE=${2:-CPU}

if [[ $3 == "display" ]] || [[ -z $3 ]]; then
  SINK_ELEMENT="gvawatermark ! videoconvert ! gvafpscounter ! autovideosink sync=false"
elif [[ $3 == "fps" ]]; then
  SINK_ELEMENT="gvafpscounter ! fakesink async=false "
else
  echo Error wrong value for SINK_ELEMENT parameter
  echo Possible values: display - render, fps - show FPS only
  exit
fi

MODEL1=face-detection-adas-0001
MODEL2=age-gender-recognition-retail-0013
MODEL3=emotions-recognition-retail-0003
MODEL4=landmarks-regression-retail-0009

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

DETECT_MODEL_PATH=${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml
CLASS_MODEL_PATH=${MODELS_PATH}/intel/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml
CLASS_MODEL_PATH1=${MODELS_PATH}/intel/emotions-recognition-retail-0003/FP32/emotions-recognition-retail-0003.xml
CLASS_MODEL_PATH2=${MODELS_PATH}/intel/landmarks-regression-retail-0009/FP32/landmarks-regression-retail-0009.xml

MODEL2_PROC=$(PROC_PATH $MODEL2)
MODEL3_PROC=$(PROC_PATH $MODEL3)
MODEL4_PROC=$(PROC_PATH $MODEL4)

PIPELINE="gst-launch-1.0 $SOURCE_ELEMENT ! decodebin ! \
gvadetect model=$DETECT_MODEL_PATH device=$DEVICE ! queue ! \
gvaclassify model=$CLASS_MODEL_PATH model-proc=$MODEL2_PROC device=$DEVICE ! queue ! \
gvaclassify model=$CLASS_MODEL_PATH1 model-proc=$MODEL3_PROC device=$DEVICE ! queue ! \
gvaclassify model=$CLASS_MODEL_PATH2 model-proc=$MODEL4_PROC device=$DEVICE ! queue ! \
$SINK_ELEMENT"

echo ${PIPELINE}
$PIPELINE
