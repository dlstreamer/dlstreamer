#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

# Usage:
#   ./vehicle_detection_publish_mqtt_multi.sh <source_pathfile> <num_concurrent_pipelines>
# Example:
#   ./vehicle_detection_publish_mqtt_multi.sh $VIDEO_EXAMPLES_DIR/Pexels_Videos_4786.mp4 10

BASEDIR=$(dirname "$0")/../../..
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh

source $BASEDIR/scripts/path_extractor.sh

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./vehicle_detection_publish_mqtt_multi.sh <source_video_pathfile> <num_concurrent_pipelines>"
  exit
fi

FILE=${1}

MODEL=vehicle-license-plate-detection-barrier-0106
PRE_PROC=ie
#Force to use original 'master' plugins
#GST_PLUGIN_PATH=/usr/local/lib/gst-video-analytics
# We will generate OUTFILE per instance
OUTFILE=${2:-"metapublish_report.json"}
OUTFORMAT=${3:-"batch"}  # values: batch, stream

if [ -z "$2" ]; then
  LAST_INSTANCE=1
else
  LAST_INSTANCE=$2
fi

if [ -z "${4}" ]; then
   echo "Using BUILD path for built on this branch/docker image."
   GST_PLUGIN_PATH=/root/gst-video-analytics/build/intel64/Release/lib
 else
   #Any fourth param value forces use of original 'master' plugins
   echo "Using ORIGINAL plugins built on this branch/docker image."
   GST_PLUGIN_PATH=/usr/local/lib/gst-video-analytics
fi

# Optionally, add the following to gvametaconvert to forces
# Distinct count of inferences on any video...
#FORCE_DETECTIONS=include-no-detections=true

echo "Will schedule [$LAST_INSTANCE] vehicle_detection instances. Expect XXX total frames per instance (for source Pexels.mp4)."

for ((INSTANCE=1; INSTANCE<=$LAST_INSTANCE; INSTANCE++)); do
   echo "Scheduling vehicle_detection instance #$INSTANCE / $LAST_INSTANCE..."

    #build mqtt output clientid
    today="$( date +"%Y%m%d-%H-%M-%S" )"
    clientIdValue="clientid-$INSTANCE-$today"
    clientIdValue2="clientid-$INSTANCE-P2-$today"

    DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL)

    echo "Sleeping for 5 seconds to offset requests..."
    sleep 5

    # Note that two pipelines create instances of singleton element 'inf0', so we can specify parameters only in first instance
    gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
                    filesrc location=$FILE ! decodebin ! video/x-raw ! videoconvert ! \
                    gvadetect inference-id=inf0 model=$DETECT_MODEL_PATH device=CPU pre-proc=$PRE_PROC every-nth-frame=1 batch-size=1 ! queue ! \
                    gvametaconvert ${FORCE_DETECTIONS} converter=json method=all ! \
                    gvametapublish method=mqtt address=127.0.0.1:1883 clientid=$clientIdValue topic="MQTTExamples" timeout=1000 ! \
                    queue ! gvawatermark ! videoconvert ! fakesink \
                    filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! gvadetect inference-id=inf0 ! \
                    gvametaconvert ${FORCE_DETECTIONS} converter=json method=all ! \
                    gvametapublish method=mqtt address=127.0.0.1:1883 clientid=$clientIdValue2 topic="MQTTExamples" timeout=1000 ! \
                    queue ! gvawatermark ! videoconvert ! fakesink  &
done
