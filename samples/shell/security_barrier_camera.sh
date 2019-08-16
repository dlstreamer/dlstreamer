#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

BASEDIR=$(dirname "$0")/../..
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh

#import GET_MODEL_PATH and PROC_PATH
source $BASEDIR/scripts/path_extractor.sh

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./security_barrier_camera.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

MODEL1=vehicle-license-plate-detection-barrier-0106
MODEL2=vehicle-attributes-recognition-barrier-0039
MODEL3=license-plate-recognition-barrier-0001

MODEL1_PROC=vehicle-license-plate-detection-barrier-0106
MODEL2_PROC=vehicle-attributes-recognition-barrier-0039
MODEL3_PROC=license-plate-recognition-barrier-0001

DEVICE=CPU

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL1 )
CLASS_MODEL_PATH=$(GET_MODEL_PATH $MODEL2 )
CLASS_MODEL_PATH1=$(GET_MODEL_PATH $MODEL3 )

echo Running sample with the following parameters:
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}
echo LD_LIBRARY_PATH=${LD_LIBRARY_PATH}

# Models used in this sample support only default batch-size=1.
# If you try to set a different batch-size, the initialization of GVA plugins will not be successful.
gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
  filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! \
  gvadetect   model=$DETECT_MODEL_PATH model-proc=$(PROC_PATH $MODEL1_PROC) device=$DEVICE ! queue ! \
  gvaclassify model=$CLASS_MODEL_PATH  model-proc=$(PROC_PATH $MODEL2_PROC) device=$DEVICE object-class=vehicle ! queue ! \
  gvaclassify model=$CLASS_MODEL_PATH1 model-proc=$(PROC_PATH $MODEL3_PROC) device=$DEVICE object-class=license-plate ! queue ! \
  gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
