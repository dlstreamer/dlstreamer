#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================
set -e

INPUT=${1:-how_are_you_doing.wav}

if [ -z "$AUDIO_MODELS_PATH" ]
then
  AUDIO_MODELS_PATH=models
fi

if [ ! -d "$AUDIO_MODELS_PATH" ]
then
  echo -e "\e[31mERROR: Invalid model directory $AUDIO_MODELS_PATH , execute download_audio_models.sh to download models\e[0m" 1>&2
  exit 1
fi

DEVICE=CPU
MODEL=aclnet_des_53_fp32

if [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi


GET_MODEL_PATH() {
    model_name=$1
    for models_dir in ${AUDIO_MODELS_PATH//:/ }; do
        paths=$(find $models_dir -type f -name "*$model_name.xml" -print)
        if [ ! -z "$paths" ];
        then
            considered_precision_paths=$(echo "$paths" | grep "/$PRECISION/")
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

    echo -e "\e[31mModel $model_name file was not found. Please set AUDIO_MODELS_PATH\e[0m" 1>&2
    exit 1
}

PROC_PATH() {
    AUDIO_MODEL_PROC_PATH=$(dirname "$0")/model_proc/$1.json
    if [ ! -f "$AUDIO_MODEL_PROC_PATH" ]
    then
      echo -e "\e[31mERROR: Invalid model-proc file path $AUDIO_MODEL_PROC_PATH\e[0m" 1>&2
      exit 1
    fi
    echo $AUDIO_MODEL_PROC_PATH
}

MODEL_PATH=$(GET_MODEL_PATH $MODEL)

MODEL_PROC_PATH=$(PROC_PATH $MODEL)

PIPELINE="gst-launch-1.0 $SOURCE_ELEMENT ! \
decodebin ! audioresample ! audioconvert ! audio/x-raw, channels=1,format=S16LE,rate=16000 ! audiomixer output-buffer-duration=100000000 ! \
gvaaudiodetect model=$MODEL_PATH model-proc=$MODEL_PROC_PATH sliding-window=0.2 \
! gvametaconvert ! gvametapublish file-format=json-lines ! \
fakesink"

echo ${PIPELINE}
${PIPELINE}