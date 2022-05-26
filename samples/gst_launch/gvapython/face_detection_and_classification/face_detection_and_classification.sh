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

SCRIPTDIR="$(dirname "$(realpath "$0")")"
PYTHON_SCRIPT1=$SCRIPTDIR/postproc_callbacks/ssd_object_detection.py
PYTHON_SCRIPT2=$SCRIPTDIR/postproc_callbacks/age_gender_classification.py
PYTHON_SCRIPT3=$SCRIPTDIR/postproc_callbacks/age_logger.py

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

DETECT_MODEL_PATH=${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml
CLASS_MODEL_PATH=${MODELS_PATH}/intel/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}

PIPELINE="gst-launch-1.0 \
$SOURCE_ELEMENT ! decodebin ! \
gvainference model=$DETECT_MODEL_PATH device=$DEVICE ! queue ! \
gvapython module=$PYTHON_SCRIPT1 ! \
gvainference inference-region=roi-list model=$CLASS_MODEL_PATH device=$DEVICE ! queue ! \
gvapython module=$PYTHON_SCRIPT2 ! \
gvapython module=$PYTHON_SCRIPT3 class=AgeLogger function=log_age kwarg={\\\"log_file_path\\\":\\\"/tmp/age_log.txt\\\"} ! \
$SINK_ELEMENT"

echo ${PIPELINE}
PYTHONPATH=$PYTHONPATH:$(dirname "$0")/../../../../python \
$PIPELINE
