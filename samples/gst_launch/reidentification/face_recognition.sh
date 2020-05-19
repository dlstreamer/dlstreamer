#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

GET_MODEL_PATH() {
    model_name=$1
    precision=${2:-"FP32"}
    for models_dir in ${MODELS_PATH//:/ }; do
        paths=$(find $models_dir -type f -name "*$model_name.xml" -print)
        if [ ! -z "$paths" ];
        then
            considered_precision_paths=$(echo "$paths" | grep "/$precision/")
            if [ ! -z "$considered_precision_paths" ];
            then
                echo $(echo "$considered_precision_paths" | head -n 1)
                exit 0
            else
                echo $(echo "$paths" | head -n 1)
                exit 0
            fi
        fi
    done

    echo -e "\e[31mModel $model_name file was not found. Please set MODELS_PATH\e[0m" 1>&2
    exit 1
}

PROC_PATH() {
    echo ./model_proc/$1.json
}

INPUT=$1
GALLERY=${2:-"./gallery/gallery.json"}

if [ -z ${INPUT} ]
  then
    echo -e "\e[31mPlease specify path to the input file\e[0m" 1>&2
    exit 1
fi

if [ ! -f ${GALLERY} ]; then
  echo -e "\e[31mCan't find gallery: ${GALLERY}\e[0m" 1>&2
  exit 1
fi

DETECTION_MODEL=face-detection-adas-0001
LANDMARKS_MODEL=landmarks-regression-retail-0009
IDENTIFICATION_MODEL=face-reidentification-retail-0095

LANDMARKS_MODEL_PROC=landmarks-regression-retail-0009
IDENTIFICATION_MODEL_PROC=face-reidentification-retail-0095

DEVICE=CPU
PRE_PROC=opencv

DETECT_MODEL_PATH=$(GET_MODEL_PATH $DETECTION_MODEL )
LANDMARKS_MODEL_PATH=$(GET_MODEL_PATH $LANDMARKS_MODEL )
IDENTIFICATION_MODEL_PATH=$(GET_MODEL_PATH $IDENTIFICATION_MODEL )

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

echo -e "\e[32mRunning sample with the following parameters:\e[0m"
echo GST_PLUGIN_PATH=${GST_PLUGIN_PATH}

PIPELINE="gst-launch-1.0 \
          ${SOURCE_ELEMENT} ! decodebin ! videoconvert ! video/x-raw,format=BGRx ! \
          gvadetect model=$DETECT_MODEL_PATH device=$DEVICE pre-process-backend=$PRE_PROC ! queue ! \
          gvaclassify reclassify-interval=5 model=$LANDMARKS_MODEL_PATH model-proc=$(PROC_PATH $LANDMARKS_MODEL_PROC) device=$DEVICE pre-process-backend=$PRE_PROC ! queue ! \
          gvaclassify reclassify-interval=5 model=$IDENTIFICATION_MODEL_PATH model-proc=$(PROC_PATH $IDENTIFICATION_MODEL_PROC) device=$DEVICE pre-process-backend=$PRE_PROC ! queue ! \
          gvaidentify gallery=${GALLERY} ! queue ! \
          gvawatermark ! videoconvert ! fpsdisplaysink video-sink=xvimagesink sync=false"
echo -e "\e[32mPipeline:\e[0m"
echo ${PIPELINE}

echo -e "\e[32mLaunch\e[0m"
${PIPELINE}
