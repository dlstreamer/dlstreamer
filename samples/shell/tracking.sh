#!/bin/bash
# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# "This sample demonstrates how to track people and vehicles on a video stream with GVA"
# "Tracker is especially useful for videos without frequent scene changes"
# "Usage: ./tracking.sh EVERY_NTH_FRAME FILE"
# "./traking.sh 5 /home/user/video.mp4"
# "EVERY_NTH_FRAME    Detection on every nth frame only(int)"
# "FILE               Path to mp4 or h264 file"

set -e

BASEDIR=$(dirname "$0")/../..
#import GET_MODEL_PATH and PROC_PATH
source $BASEDIR/scripts/path_extractor.sh
if [ -n ${GST_SAMPLES_DIR} ]; then
    # set up GST environment
    source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh

# parse input parameters
EVERY_NTH_FRAME=${1}
FILE=${2}

# define the models used for detection and classification
DETECTION_MODEL=person-vehicle-bike-detection-crossroad-0078
PERSON_CLASSIFICATION_MODEL=person-attributes-recognition-crossroad-0230
VEHICLE_CLASSIFICATION_MODEL=vehicle-attributes-recognition-barrier-0039

# define parameters for inference
# tracking type: short-term, zero-term, iou
# use short-term tracker for every_nth_frame > 1
TRACKING_TYPE="short-term"
SKIP_INTERVAL=10

# pipeline scheme
# gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
#              VIDEO_SOURCE \
#              DETECTION ! queue ! \
#              TRACKING ! queue ! \
#              PERSON_CLASSIFICATION  ! queue ! \
#              VEHICLE_CLASSIFICATION ! queue ! \
#              VIDEO_SINK

# launch pipeline
gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
  filesrc location=${FILE} ! decodebin ! videoconvert ! video/x-raw,format=BGRx ! \
  gvadetect model=$(GET_MODEL_PATH $DETECTION_MODEL) \
            model-proc=$(PROC_PATH $DETECTION_MODEL) \
            every-nth-frame=${EVERY_NTH_FRAME} \
            device=CPU pre-proc=ie ! \
  queue ! \
  gvatrack tracking-type=${TRACKING_TYPE} ! \
  queue ! \
  gvaclassify model=$(GET_MODEL_PATH $PERSON_CLASSIFICATION_MODEL) \
              model-proc=$(PROC_PATH $PERSON_CLASSIFICATION_MODEL) \
              skip-classified-objects=yes \
              skip-interval=${SKIP_INTERVAL} \
              device=CPU pre-proc=ie object-class=person ! \
  queue ! \
  gvaclassify model=$(GET_MODEL_PATH $VEHICLE_CLASSIFICATION_MODEL) \
              model-proc=$(PROC_PATH $VEHICLE_CLASSIFICATION_MODEL) \
              skip-classified-objects=yes \
              skip-interval=${SKIP_INTERVAL} \
              device=CPU pre-proc=ie object-class=vehicle ! \
  queue ! \
  gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false  