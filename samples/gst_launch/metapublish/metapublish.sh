#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2021 Intel Corporation
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

PROC_PATH() {
    echo $(dirname "$0")/model_proc/$1.json
}

DETECT_MODEL_PATH=${MODELS_PATH}/intel/face-detection-adas-0001/FP32/face-detection-adas-0001.xml
CLASS_MODEL_PATH=${MODELS_PATH}/intel/age-gender-recognition-retail-0013/FP32/age-gender-recognition-retail-0013.xml
MODEL2_PROC=$(PROC_PATH  $MODEL2)

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
