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
  echo "Usage: ./vehicle_detection_publish_file.sh <path/to/your/video/sample>"
  exit
fi

FILE=${1}

MODEL=vehicle-license-plate-detection-barrier-0106
PRE_PROC=ie

# This sample may be invoked with a variety of output targets to demonstrate/confirm expected behavior:
# 1. For named pipe, the file must be created before we launch a pipeline that uses the gvametapublish element.
#    Otherwise, gvametapublish will create and persist a standard file to store the inference messages.
#    e.g., usage:
#      mkfifo /tmp/testfifo
#      tail -f /tmp/testfifo
#      ./vehicle_detection_publish_file_fifo_multi.sh /root/video-examples/Pexels_Videos_4786.mp4  /tmp/testfifo
#
# 2. For stdout, we may pass either "stdout" or "/dev/stdout". This is the default used in this example.
#    Note that when not supplied as a parameter to this script, the stdout and stderr output gets redirected
#    to files named /tmp/outfile1.txt and /tmp/errorfile1.txt.
#    e.g., usage:
#      ./vehicle_detection_publish_file_fifo_multi.sh /root/video-examples/Pexels_Videos_4786.mp4
OUTFILE_1=${2:-"/dev/stdout"}
OUTFILE_2=${2:-"/dev/stderr"}
# 3. For other files, the behavior is that gvametapublish will create the file if it does not exist.
#    If outputformat=stream, gvametapublish will append records to existing fifo/files.
#    If outputformat=batch, gvametapublish overwrites/replaces content.

OUTFORMAT=${3:-"stream"}  # values: batch, stream

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL)

# Note that two pipelines create instances of singleton element 'inf0', so we can specify parameters only in first instance
  gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
      filesrc location=$FILE ! decodebin ! video/x-raw ! videoconvert ! \
          gvadetect model-instance-id=inf0 model=$DETECT_MODEL_PATH device=CPU pre-process-backend=$PRE_PROC inference-interval=1 batch-size=1 ! queue ! \
          gvametaconvert ${FORCE_DETECTIONS} format=json add-tensor-data=true ! \
          gvametapublish method=file file-path=${OUTFILE_1} file-format=${OUTFORMAT} ! \
          queue ! gvawatermark ! videoconvert ! fakesink \
      filesrc location=${FILE} ! decodebin ! video/x-raw ! videoconvert ! gvadetect model-instance-id=inf0 ! \
          gvametaconvert ${FORCE_DETECTIONS} format=json add-tensor-data=true ! \
          gvametapublish method=file file-path=${OUTFILE_2} file-format=${OUTFORMAT} ! \
          queue ! gvawatermark ! videoconvert ! fakesink > /tmp/outfile1.txt 2>/tmp/errorfile1.txt
