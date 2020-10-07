#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e
INPUT=${1:-https://github.com/intel-iot-devkit/sample-videos/raw/master/head-pose-face-detection-female-and-male.mp4}
METHOD=${2:-file} # Required - Accepts: file, fifo, kafka, mqtt
OUTPUT=${3} # Default: empty indicates stdout for file, localhost:9092 for kafka, and localhost:1883 for mqtt; Accepts (method==file) absolte path to file, (method==kafka/mqtt) host and port of message broker,
TOPIC=${4:-dlstreamer} # Required for kafka and mqtt

if [ "$METHOD" != "file" ] && [ "$TOPIC" == "" ]; then
    echo "Usage: metapublish.sh [INPUT] [METHOD] [OUTPUT] [TOPIC]"
    echo " where"
    echo "    INPUT is full path location to source video."
    echo "    METHOD is one of 'file', 'kafka', 'mqtt'"
    echo "    OUTPUT is absolute path to file/FIFO or the MQTT/Kafka broker address"
    echo "    TOPIC is MQTT/Kafka topic"
    exit
fi

OUTPUT_PROPERTY=""
if [[ "$METHOD" == "file" ]]; then
    FILE_FORMAT="json" # Use json for persistent files, json-lines for FIFO files
    if [ "${OUTPUT}" != "" ]; then # default output file is stdout - print to console
        OUTPUT_PROPERTY="file-path=${OUTPUT} file-format=$FILE_FORMAT"
    fi
else
    if [ "${OUTPUT}" == "" ]; then
        TOPIC="dlstreamer"
        if [[ "$METHOD" == "mqtt" ]]; then
             OUTPUT="localhost:1883"
        elif [[ "$METHOD" == "kafka" ]]; then
             OUTPUT="localhost:9092"
        fi
    fi
    OUTPUT_PROPERTY="address=$OUTPUT topic=$TOPIC"
fi

MODEL=face-detection-adas-0001
MODEL2=age-gender-recognition-retail-0013
PRECISION="FP32"
DEVICE=CPU
METACONVERT_INDENT=4 # Number of spaces to indent pretty json

if [[ $INPUT == "/dev/video"* ]]; then
  SOURCE_ELEMENT="v4l2src device=${INPUT}"
elif [[ $INPUT == *"://"* ]]; then
  SOURCE_ELEMENT="urisourcebin buffer-size=4096 uri=${INPUT}"
else
  SOURCE_ELEMENT="filesrc location=${INPUT}"
fi

GET_MODEL_PATH() {
    model_name=$1
    for models_dir in ${MODELS_PATH//:/ }; do
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

    echo -e "\e[31mModel $model_name file was not found. Please set MODELS_PATH\e[0m" 1>&2
    exit 1
}

PROC_PATH() {
    echo "$(dirname '$1')/model_proc/$2.json"
}

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )
CLASS_MODEL_PATH=$(GET_MODEL_PATH $MODEL2)
echo "MODEL2: ${MODEL2}"
MODEL2_PROC=$(PROC_PATH "./" "$MODEL2")
echo "MODEL2_PROC: $MODEL2_PROC"
echo $SOURCE_ELEMENT

PIPELINE="gst-launch-1.0 $SOURCE_ELEMENT ! \
decodebin ! \
gvadetect model=$DETECT_MODEL_PATH device=$DEVICE ! queue ! \
gvaclassify model=$CLASS_MODEL_PATH model-proc=$MODEL2_PROC device=$DEVICE ! queue ! \
gvametaconvert json-indent=$METACONVERT_INDENT ! \
gvametapublish method=$METHOD $OUTPUT_PROPERTY ! \
fakesink sync=false"

echo ${PIPELINE}
#LAUNCH
${PIPELINE}
