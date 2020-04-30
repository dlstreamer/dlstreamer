#!/bin/bash

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
FILE=${1}
EVERY_NTH_FRAME=${2:-10}

DETECTION_MODEL=person-detection-retail-0013
HUMAN_POSE_MODEL=human-pose-estimation-0001
TRACKING_TYPE="short-term"

gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
  filesrc location=${FILE} ! decodebin ! videoconvert ! video/x-raw,format=BGR ! \
  gvadetect model=$(GET_MODEL_PATH $DETECTION_MODEL) \
            model-proc=$(PROC_PATH $DETECTION_MODEL) \
            every-nth-frame=${EVERY_NTH_FRAME} \
            device=CPU pre-proc=ie ! \
  queue ! \
  gvatrack tracking-type=${TRACKING_TYPE} ! \
  queue ! \
  gvaskeleton model_path=$(GET_MODEL_PATH $HUMAN_POSE_MODEL) ! \
  gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false
