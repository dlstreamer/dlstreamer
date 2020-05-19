#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}

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

PYTHONPATH=$PYTHONPATH:$(dirname "$0")/../../../python \
python3 $(dirname "$0")/draw_face_attributes.py -i ${INPUT} -d ${PATH_D} -c1 ${PATH_C1} -c2 ${PATH_C2} -c3 ${PATH_C3}
