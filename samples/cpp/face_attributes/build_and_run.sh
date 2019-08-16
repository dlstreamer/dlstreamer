#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

BASEDIR=$(dirname "$0")/../../..

if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi

$BASEDIR/scripts/build.sh

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./build_and_run.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

$BASEDIR/build/intel64/Release/bin/face_attributes -i $FILE
