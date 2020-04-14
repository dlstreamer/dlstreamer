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

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage: ./vehicle_detection_publish_mqtt.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

MODEL=vehicle-license-plate-detection-barrier-0106

DEVICE=CPU
PRE_PROC=ie

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL )

if ldconfig -p | grep -q libpaho-mqtt3c
then
	echo "Paho MQTT library found";
else
	echo "No Paho MQTT library found, please run the install_dependencies.sh script in the scripts folder";
	exit 1
fi

paho_c_pub test -h 127.0.0.1 -p 1883 -m Hello & processID=$!
sleep 10

if ps -p $processID > /dev/null
then
   echo "Error, paho_c_pub process should have connected, published a message, and closed the connection. Unable to establish a connection likely. Killing the process"
   kill $processID
   exit 1
else
   echo "Able to establish connection to mqtt server"
fi

# Note that two pipelines create instances of singleton element 'inf0', so we can specify parameters only in first instance
gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
                filesrc location=$FILE ! decodebin ! video/x-raw ! videoconvert ! \
                gvadetect model-instance-id=inf0 model=$DETECT_MODEL_PATH device=$DEVICE pre-process-backend=$PRE_PROC inference-interval=1 batch-size=1 ! queue ! \
                gvawatermark ! videoconvert ! fakesink \
                filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! gvadetect model-instance-id=inf0 ! \
                gvametaconvert format=json ! \
                gvametapublish method=mqtt address=127.0.0.1:1883 mqtt-client-id=clientIdValue topic="MQTTExamples" timeout=1000 ! \
                queue ! gvawatermark ! videoconvert ! fakesink

#gvametapublish method=kafka address=127.0.0.1:9092 topic=mytopic67 ! \
