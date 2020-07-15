#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

VIDEO_EXAMPLES_PATH=""
INTEL_MODELS_PATH=""
MODELS_PATH=""
IMAGE_NAME=""

for i in "$@"
do
case $i in
    -h|--help)
    echo "usage: sudo ./run_docker_container.sh [--video-examples-path=<path>]"
    echo "[--intel-models-path=<path>] [--models-path=<path>] [--image-name=<name>]"
    exit 0
    ;;
    --video-examples-path=*)
    VIDEO_EXAMPLES_PATH="${i#*=}"
    shift
    ;;
    --intel-models-path=*)
    INTEL_MODELS_PATH="${i#*=}"
    shift
    ;;
    --models-path=*)
    MODELS_PATH="${i#*=}"
    shift
    ;;
    --image-name=*)
    IMAGE_NAME="${i#*=}"
    shift
    ;;
    *)
          # unknown option
    ;;
esac
done

xhost local:root
docker run -it --privileged --net=host \
    -v ~/.Xauthority:/root/.Xauthority \
    -v /tmp/.X11-unix/:/tmp/.X11-unix/ \
    -e DISPLAY=$DISPLAY \
    -e HTTP_PROXY=$HTTP_PROXY \
    -e HTTPS_PROXY=$HTTPS_PROXY \
    -e http_proxy=$http_proxy \
    -e https_proxy=$https_proxy \
    \
    -v $INTEL_MODELS_PATH:/root/intel_models \
    -v $MODELS_PATH:/root/models \
    -e MODELS_PATH=/root/intel_models:/root/models \
    \
    -v $VIDEO_EXAMPLES_PATH:/root/video-examples \
    -e VIDEO_EXAMPLES_DIR=/root/video-examples \
    \
    -w /root/gst-video-analytics/samples \
    $IMAGE_NAME
