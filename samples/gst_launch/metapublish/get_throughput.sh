#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Purpose: Gets rough throughput performance of GST pipeline
# output using gvametapublish to file/pipe.
# Use plugins built/installed for the container
#export GST_PLUGIN_PATH=/usr/lib/gst-video-analytics/
#export GST_PLUGIN_PATH=/root/gst-video-analytics/build/intel64/Release/lib

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

INPUT_FILE=${1}

MODEL=vehicle-license-plate-detection-barrier-0106
PRE_PROC=ie

# AutoName output file:
prefix="/tmp/result-"
today="$( date +"%Y%m%d-%H-%M-%S-%N" )"

number=0
fname="$prefix$today.txt"
while [ -e "$fname" ]; do
    printf -v fname -- '%s%s-%02d.txt' "$prefix" "$today" "$(( ++number ))"
done
printf 'Using "%s" as input video source\n' "$INPUT_FILE"

#Try via file stream/batch
OUTFILE_1=${2:-"$fname"}

#Try via stdout/stderr
#OUTFILE_1=${2:-"/dev/stderr"}
#OUTFILE_1=${2:-"/dev/stdout"}

#Try via Named pipe
#OUTFILE_1=${2:-"/tmp/testfifo"}
#rm $OUTFILE_1
#mkfifo $OUTFILE_1
printf 'Using "%s" to store results\n' "$OUTFILE_1"

OUTFORMAT=${3:-"stream"}  # values: batch, stream

DETECT_MODEL_PATH=$(GET_MODEL_PATH $MODEL)
FORCE_DETECTIONS="add-empty-results=true"

printf 'Using "%s" as model\n' "$DETECT_MODEL_PATH"

# Record rough start time
before="$( date +"%Y%m%d %H:%M:%S.%N" )"
echo .
echo StartTime: $before

# Launch pipeline
  gst-launch-1.0 --gst-plugin-path ${GST_PLUGIN_PATH} \
      filesrc location=$INPUT_FILE ! decodebin ! video/x-raw ! videoconvert ! \
          gvadetect model-instance-id=inf0 model=$DETECT_MODEL_PATH device=CPU pre-process-backend=$PRE_PROC inference-interval=1 batch-size=1 ! queue ! \
          gvametaconvert format=json ${FORCE_DETECTIONS}  ! \
          gvametapublish method=file file-path=${OUTFILE_1} file-format=${OUTFORMAT} ! \
          queue ! gvawatermark ! videoconvert ! fakesink > /tmp/stdout1.txt 2>/tmp/stderr1.txt

# Record rough complete time
after="$( date +"%Y%m%d %H:%M:%S.%N" )"
echo .
echo CompletedTime: $after

# Calculate delta
  secdiff=$((
    $(date -d "$after" +%s) - $(date -d "$before" +%s)
  ))
  nanosecdiff=$((
    $(date -d "$after" +%N) - $(date -d "$before" +%N)
  ))
throughput_msec=$((  (secdiff * 1000) + (nanosecdiff / 1000000) ))

# Show original file name that holds creation seconds, and last write time on same file
echo .
echo "Output File Attributes:"
ls -la --full-time ${OUTFILE_1}

# Output throughput and number of inference detections streamed to file:
inf_count=$( wc -l ${OUTFILE_1} )
echo .
echo .
echo "gvametapublish emitted $inf_count inferences in $throughput_msec ms."
