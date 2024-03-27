#!/bin/bash
# ==============================================================================
# Copyright (C) 2020-2024 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

PIPELINE=${1}
CHANNELS_COUNT=${2:-1}
PROCESSES_COUNT=${3:-1}

if (( CHANNELS_COUNT <= 0 || PROCESSES_COUNT < 0 || CHANNELS_COUNT < PROCESSES_COUNT )); then
  echo "Usage: ./gst-launch-multi.sh GST_PIPELINE [NUMBER_STREAMS] [NUMBER_PROCESSES]"
  exit
fi

if (( PROCESSES_COUNT > 1 )); then
  FPS_PIPE=/tmp/fps
  PIPELINE=${PIPELINE/"gvafpscounter"/"gvafpscounter write-pipe=${FPS_PIPE}"}
fi

CHANNELS_PER_PROCESS=$((CHANNELS_COUNT / PROCESSES_COUNT))
CHANNELS_REMAINDER=$((CHANNELS_COUNT % PROCESSES_COUNT))

SOCKETS_COUNT=$(grep physical.id /proc/cpuinfo | sort -u | wc -l)
PROCESSES_PER_SOCKET=$((PROCESSES_COUNT / SOCKETS_COUNT))
PROCESSES_REMAINDER=$((PROCESSES_COUNT % SOCKETS_COUNT))

generate_pipeline() {
  if (( PROCESSES_COUNT > 1 && SOCKETS_COUNT > 1 )); then
    FINAL_PIPELINE_STR+="numactl --cpunodebind=$CURRENT_SOCKETS_COUNT --membind=$CURRENT_SOCKETS_COUNT gst-launch-1.0"
  else
    FINAL_PIPELINE_STR+="gst-launch-1.0"
  fi
  for (( CURRENT_CHANNELS_COUNT=0; CURRENT_CHANNELS_COUNT < CHANNELS_PER_PROCESS; ++CURRENT_CHANNELS_COUNT )); do
    FINAL_PIPELINE_STR+=" $PIPELINE"
  done
  if (( "$CHANNELS_REMAINDER" != 0 )); then
    FINAL_PIPELINE_STR+=$PIPELINE
    CHANNELS_REMAINDER=$((CHANNELS_REMAINDER - 1))
  fi
  if (( "$PROCESSES_COUNT" != 1 )); then
    FINAL_PIPELINE_STR+="& "
  fi
}

FINAL_PIPELINE_STR=""
for (( CURRENT_SOCKETS_COUNT=0; CURRENT_SOCKETS_COUNT < SOCKETS_COUNT; ++CURRENT_SOCKETS_COUNT )); do
  for (( CURRENT_PROCESSES_COUNT=0; CURRENT_PROCESSES_COUNT < PROCESSES_PER_SOCKET; ++CURRENT_PROCESSES_COUNT )); do
    generate_pipeline
  done

  if (( "$PROCESSES_REMAINDER" != 0 )); then
    generate_pipeline
    PROCESSES_REMAINDER=$((PROCESSES_REMAINDER - 1))
  fi
done

# Kill all child processes on Ctrl-C
cleanup() {
  pkill -P $$
}
trap cleanup SIGINT

# Outputting final pipeline and launching it
#echo ${FINAL_PIPELINE_STR}
eval "${FINAL_PIPELINE_STR}"

# Additional process to collect FPS data from other processes and wait for completion
if (( "$PROCESSES_COUNT" > 1 )); then
  eval "gst-launch-1.0 gvafpscounter read-pipe=${FPS_PIPE} interval=1 ! fakesink"
fi
