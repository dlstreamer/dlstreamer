#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

if [ -z ${1} ]; then
  echo "ERROR set path to video"
  echo "Usage : ./benchmark.sh VIDEO_FILE [DECODE_DEVICE] [INFERENCE_DEVICE] [CHANNELS_COUNT] [PROCESSES_COUNT] [DECODE_PIPELINE]"
  echo "You can download video with"
  echo "\"curl https://raw.githubusercontent.com/intel-iot-devkit/sample-videos/master/head-pose-face-detection-female-and-male.mp4 --output /path/to/your/video/head-pose-face-detection-female-and-male.mp4\""
  echo "and run sample ./benchmark.sh /path/to/your/video/head-pose-face-detection-female-and-male.mp4"
  exit
fi

VIDEO_FILE_NAME=${1}
DECODE_DEVICE=${2:-CPU}
INFERENCE_DEVICE=${3:-CPU}
CHANNELS_COUNT=${4:-1}
PROCESSES_COUNT=${5:-1}
DECODE_PIPELINE=${6:-decodebin}

if [ $CHANNELS_COUNT -le 0 ]; then
  echo "ERROR: wrong value for CHANNELS_COUNT parameter"
  echo "Possible value must be > 0"
  exit
fi

if [ $CHANNELS_COUNT -lt $PROCESSES_COUNT ]; then
  echo "ERROR: wrong value for CHANNELS_COUNT parameter"
  echo "Possible value must be >= PROCESSES_COUNT"
  exit
fi

if [ $PROCESSES_COUNT -le 0 ]; then
  echo "ERROR: wrong value for PROCESSES_COUNT parameter"
  echo "Possible value must be > 0"
  exit
fi

MODEL=face-detection-adas-0001
DETECT_MODEL_PATH=${MODELS_PATH}/intel/${MODEL}/FP32/${MODEL}.xml

# Choosing a decoder(default, custom)
DECODER=""
if [ "$DECODE_PIPELINE" == decodebin ]; then
  if [ $DECODE_DEVICE == CPU ]; then
    DECODER+="decodebin force-sw-decoders=true ! video/x-raw"
  else
    DECODER+="decodebin ! video/x-raw\(memory:DMABuf\)"
  fi
else
  if [ $DECODE_DEVICE == CPU ]; then
    DECODER+="$DECODE_PIPELINE ! video/x-raw"
  else
    DECODER+="$DECODE_PIPELINE ! video/x-raw\(memory:DMABuf\)"
  fi
fi

# Selecting read-write pipes gvafpscounter if the number of processes > 1
FPS_COUNTER="gvafpscounter"
if [ $PROCESSES_COUNT -gt 1 ]; then
  FPS_COUNTER="gvafpscounter write-pipe=/tmp/fps"
fi

# Obtaining the necessary parameters and calculating new
CORES=`nproc`
SOCKETS_COUNT=`grep physical.id /proc/cpuinfo | sort -u | wc -l`

CHANNELS_PER_PROCESS=$(($CHANNELS_COUNT / $PROCESSES_COUNT))
CHANNELS_REMAINDER=$(($CHANNELS_COUNT % $PROCESSES_COUNT))

PROCESSES_PER_SOCKET=$(($PROCESSES_COUNT / $SOCKETS_COUNT))
PROCESSES_REMAINDER=$(($PROCESSES_COUNT % $SOCKETS_COUNT))

# Building pipeline for multi-process or single-process mode
PARAMS=''
if [ $PROCESSES_COUNT -gt 1 ]; then
  if [ $INFERENCE_DEVICE == "CPU" ]; then
    THREADS_NUM=$((($CORES + $CORES % $PROCESSES_COUNT) / $PROCESSES_COUNT))
    if [ $THREADS_NUM -eq 0 ]; then
      THREADS_NUM=1
    fi
    NIREQ=$((2 * $CHANNELS_PER_PROCESS))
    THROUGHPUT_STREAMS=$(($CHANNELS_PER_PROCESS))
    PARAMS+="ie-config=CPU_THREADS_NUM=${THREADS_NUM},CPU_THROUGHPUT_STREAMS=${THROUGHPUT_STREAMS},CPU_BIND_THREAD=NO nireq=${NIREQ}"
  fi
fi

PIPELINE=" filesrc location=${VIDEO_FILE_NAME} ! ${DECODER} ! \
gvadetect model-instance-id=inf0 model=${DETECT_MODEL_PATH} device=${INFERENCE_DEVICE} ${PARAMS} ! queue ! \
${FPS_COUNTER} ! fakesink async=false "

generate_pipeline() {
  if [ $PROCESSES_COUNT -eq 1 ]; then
    FINAL_PIPELINE_STR+="gst-launch-1.0 "
  else
    FINAL_PIPELINE_STR+="numactl --cpunodebind=$CURRENT_SOCKETS_COUNT --membind=$CURRENT_SOCKETS_COUNT gst-launch-1.0 "
  fi
  for (( CURRENT_CHANNELS_COUNT=0; CURRENT_CHANNELS_COUNT < $CHANNELS_PER_PROCESS; ++CURRENT_CHANNELS_COUNT )); do
    FINAL_PIPELINE_STR+=$PIPELINE
  done
  if [ $CHANNELS_REMAINDER != 0 ]; then
    FINAL_PIPELINE_STR+=$PIPELINE
    CHANNELS_REMAINDER=$(($CHANNELS_REMAINDER - 1))
  fi
  if [ $PROCESSES_COUNT != 1 ]; then
    FINAL_PIPELINE_STR+="& "
  fi
}

FINAL_PIPELINE_STR=""
for (( CURRENT_SOCKETS_COUNT=0; CURRENT_SOCKETS_COUNT < $SOCKETS_COUNT; ++CURRENT_SOCKETS_COUNT )); do
  for (( CURRENT_PROCESSES_COUNT=0; CURRENT_PROCESSES_COUNT < $PROCESSES_PER_SOCKET; ++CURRENT_PROCESSES_COUNT )); do
    generate_pipeline
  done

  if [ $PROCESSES_REMAINDER != 0 ]; then
    generate_pipeline
    PROCESSES_REMAINDER=$(($PROCESSES_REMAINDER - 1))
  fi
done

# Kill all child processes on Ctrl-C
cleanup() {
  pkill -P $$
}
trap cleanup SIGINT

# Outputting final pipeline and launching it
echo ${FINAL_PIPELINE_STR}
eval ${FINAL_PIPELINE_STR}
if [ $PROCESSES_COUNT -gt 1 ]; then
  eval "gst-launch-1.0 gvafpscounter read-pipe=/tmp/fps interval=1 ! fakesink"
fi
