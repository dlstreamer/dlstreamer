#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2024 Intel Corporation
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

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}
OUTPUT=${2:-display} # Output type, valid values: display, json ,display-and-json 

PROC_PATH() {
    echo ./model_proc/"$1".json
}

PATH_D=${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml
PATH_C1=${MODELS_PATH}/intel/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml
PATH_C2=${MODELS_PATH}/intel/emotions-recognition-retail-0003/FP32/emotions-recognition-retail-0003.xml
PATH_C3=${MODELS_PATH}/intel/facial-landmarks-35-adas-0002/FP32/facial-landmarks-35-adas-0002.xml

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH="${GST_PLUGIN_PATH}"

PYTHONPATH=$PYTHONPATH:$(dirname "$0")/../../../python \
python3 "$(dirname "$0")"/draw_face_attributes.py -i "${INPUT}" -d "${PATH_D}" -c1 "${PATH_C1}" -c2 "${PATH_C2}" -c3 "${PATH_C3}" -o "${OUTPUT}"
