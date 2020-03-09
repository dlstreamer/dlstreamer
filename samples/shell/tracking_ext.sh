#!/bin/bash
# ==============================================================================
# Copyright (C) 2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

print_description() 
{
  echo "This sample demonstrates how to track people and vehicles on a video stream with GVA"
  echo "Tracker is especially useful for videos without frequent scene changes"
  echo "Usage: ./tracking_ext.sh [OPTIONS] VIDEO_SOURCE"
  echo "OPTIONS"
  echo "  -t=v, --tracking-type=v       Tracking type(none, iou, short-term, zero-term)"
  echo "  -n=v, --every-nth-frame=v     Detection on every nth frame only(int)"
  echo "                                This option is supported correctly only by short-term tracker"
  echo "  -c, --console                 Don't render a video"
  echo "  -h, --help                    Show this chapter"
  echo "VIDEO_SOURCE"
  echo "$(supported_video_sources)"
}

###############################################################################
set -e

BASEDIR=$(dirname "$0")/../..
#import GET_MODEL_PATH and PROC_PATH
source $BASEDIR/scripts/path_extractor.sh
#import utility functions
source $BASEDIR/scripts/sample_utils.sh

###############################################################################
# parse input parameters
TRACKING_TYPE="short-term"
EVERY_NTH_FRAME=
VIDEO_SOURCE=

while [ "$1" != "" ]; do
    case $1 in
        "-t="* | "--tracking-type="* )   TRACKING_TYPE=$(get_option $1)
                                         ;;
        "-n="* | "--every-nth-frame="* ) EVERY_NTH_FRAME=$(get_option $1)
                                         ;;
        "-h" | "--help" )                print_description
                                         exit 0
                                         ;;
        "-c" | "--console" )             USE_CONSOLE_OUTPUT=1
                                         ;;
        "-"* | "--"* )                   echo -e "\e[31mERROR: unrecognized option '${1}'\e[0m"
                                         echo "Try --help option for more information"
                                         exit 1
                                         ;;
        * )                              SOURCE_ELEMENT=$(get_source_element "${1}")
                                         break
                                         ;;
    esac
    shift
done

###############################################################################
# check if parameters are valid
# VIDEO_SOURCE is required
if [[ -z ${SOURCE_ELEMENT} ]]; then
  echo -e "\e[31mERROR: VIDEO_SOURCE is not set\e[0m"
  echo "Try --help option for more information"
  exit 1
fi

if [[ ${TRACKING_TYPE} != 'short-term' ]] && [[ ${EVERY_NTH_FRAME} -gt 1 ]]; then
  echo -e "\e[33mWARNING: --every-nth-frame > 1 is recommended only for short-term tracker\e[0m"
fi

###############################################################################
# detection for each frame is not nessesary if 'shor-term' tracker is used
# set EVERY_NTH_FRAME parameter to launch detection on every Nth frames only 
if [[ -z ${EVERY_NTH_FRAME} ]]; then
  if [ ${TRACKING_TYPE} == 'short-term' ]; then
    EVERY_NTH_FRAME=10
  else
    EVERY_NTH_FRAME=1 
  fi
fi

# define the models used for detection and classification
DETECTION_MODEL=person-vehicle-bike-detection-crossroad-0078
PERSON_CLASSIFICATION_MODEL=person-attributes-recognition-crossroad-0230
VEHICLE_CLASSIFICATION_MODEL=vehicle-attributes-recognition-barrier-0039

# define other parameters for inference
DEVICE=CPU
SKIP_INTERVAL=10

# set up GST environment 
if [ -n ${GST_SAMPLES_DIR} ]
then
    source $BASEDIR/scripts/setup_env.sh
fi
source $BASEDIR/scripts/setlocale.sh

# print the parameters
echo -e "\e[32mRunning sample with the following parameters:\e[0m"
echo "SOURCE_ELEMENT=${SOURCE_ELEMENT}"
echo "TRACKING_TYPE=${TRACKING_TYPE}"
echo "EVERY_NTH_FRAME=${EVERY_NTH_FRAME}"
echo "USE_CONSOLE_OUTPUT=${USE_CONSOLE_OUTPUT}"
echo "GST_PLUGIN_PATH=${GST_PLUGIN_PATH}"
echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH}"

###############################################################################
# compose the pipeline from the following parts:
# VIDEO_SOURCE - read the video stream and decode it  
VIDEO_SOURCE_STR=" ${SOURCE_ELEMENT} ! decodebin ! videoconvert ! video/x-raw,format=BGR "
# DETECTION - enable detection by including gvadetect into the pipeline
DETECTION_STR=" ! gvadetect model=$(GET_MODEL_PATH $DETECTION_MODEL) \
                            model-proc=$(PROC_PATH $DETECTION_MODEL) \
                            every-nth-frame=${EVERY_NTH_FRAME} \
                            device=$DEVICE pre-proc=ie "
# TRACKING - enable tracking by using of gvatrack 
if [[ ${TRACKING_TYPE} != "none" ]]; then
  TRACKING_STR="! gvatrack tracking-type=${TRACKING_TYPE}  ! queue "
else
  TRACKING_STR=
fi
# PERSON_CLASSIFICATION - enable classification of persons with gvaclassify
PERSON_CLASSIFICATION_STR=" ! gvaclassify skip-classified-objects=yes \
                                          skip-interval=${SKIP_INTERVAL} \
                                          model=$(GET_MODEL_PATH $PERSON_CLASSIFICATION_MODEL) \
                                          model-proc=$(PROC_PATH $PERSON_CLASSIFICATION_MODEL) \
                                          device=$DEVICE \
                                          pre-proc=ie \
                                          object-class=person "
# VEHICLE_CLASSIFICATION - enable classification of vehicles with gvaclassify
VEHICLE_CLASSIFICATION_STR=" ! gvaclassify skip-classified-objects=yes \
                                           skip-interval=${SKIP_INTERVAL} \
                                           model=$(GET_MODEL_PATH $VEHICLE_CLASSIFICATION_MODEL) \
                                           model-proc=$(PROC_PATH $VEHICLE_CLASSIFICATION_MODEL) \
                                           device=$DEVICE \
                                           pre-proc=ie \
                                           object-class=vehicle "
if [[ ${USE_CONSOLE_OUTPUT} -eq 0 ]]; then 
  # VIDEO_SINK - visualize the meta by gvawatermark and render the video stream
  VIDEO_SINK_STR=" ! gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false "
else
  # VIDEO_SINK - calculate average fps by gvafpscounter without rendering the video stream
  VIDEO_SINK_STR=" ! gvafpscounter ! fakesink "
fi

###############################################################################
# join all the parts together
PIPELINE_STR="gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
              ${VIDEO_SOURCE_STR} \
              ${DETECTION_STR} ! queue \
              ${TRACKING_STR} \
              ${PERSON_CLASSIFICATION_STR}  ! queue \
              ${VEHICLE_CLASSIFICATION_STR} ! queue \
              ${VIDEO_SINK_STR}"

# print the pipeline
echo -e "\e[32mPipeline\e[0m"
echo ${PIPELINE_STR}

# launch the pipeline
echo -e "\e[32mLaunch\e[0m"
${PIPELINE_STR}
