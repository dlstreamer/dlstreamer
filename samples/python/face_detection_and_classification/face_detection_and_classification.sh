#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

BASEDIR=$(dirname "$0")/../../..
if [ -n ${GST_SAMPLES_DIR} ]; then
  source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh
source $BASEDIR/scripts/path_extractor.sh

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./face_detection_and_classification.sh <path/to/your/video/sample>"
  exit
fi

INPUT=${1}

MODEL1=face-detection-adas-0001
MODEL2=age-gender-recognition-retail-0013

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL1)
CLASS_MODEL_PATH=$(GET_MODEL_PATH $MODEL2)

PYTHONPATH=$PYTHONPATH:$BASEDIR/python:$BASEDIR/samples/python \
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$GST_PLUGIN_PATH \
python3 $(dirname "$0")/detect_and_classify.py -i ${INPUT} -d ${DETECT_MODEL_PATH} -c ${CLASS_MODEL_PATH}
