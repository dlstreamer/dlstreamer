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
  echo "Usage: ./draw_face_attributes.sh <path/to/your/video/sample>"
  exit
fi

INPUT=${1}

MODEL_D=face-detection-adas-0001
MODEL_C1=age-gender-recognition-retail-0013
MODEL_C2=emotions-recognition-retail-0003
MODEL_C3=facial-landmarks-35-adas-0002

PATH_D=$(GET_MODEL_PATH $MODEL_D)
PATH_C1=$(GET_MODEL_PATH $MODEL_C1)
PATH_C2=$(GET_MODEL_PATH $MODEL_C2)
PATH_C3=$(GET_MODEL_PATH $MODEL_C3)

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}

PYTHONPATH=$PYTHONPATH:$BASEDIR/python:$BASEDIR/samples/python \
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$GST_PLUGIN_PATH \
python3 $(dirname "$0")/draw_face_attributes.py -i ${INPUT} -d ${PATH_D} -c1 ${PATH_C1} -c2 ${PATH_C2} -c3 ${PATH_C3}
