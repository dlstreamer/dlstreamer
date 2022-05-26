#!/bin/bash
# ==============================================================================
# Copyright (C) 2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

set -e

VIDEO_EXAMPLES_PATH=""
MODELS_PATH=""
IMAGE_NAME=""

for i in "$@"
do
case $i in
    -h|--help)
    echo "usage: sudo ./run_docker_container.sh [--video-examples-path=<path>]"
    echo "[--models-path=<path>] --image-name=<name>"
    exit 0
    ;;
    --video-examples-path=*)
    VIDEO_EXAMPLES_PATH="${i#*=}"
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

if [[ -z "$IMAGE_NAME" ]]; then
    echo "ERROR: Target docker image name is not provided"
    echo "Please provide it with '--image-name=' option"
    exit 1
fi

xhost local:root

DOCKER_HOME_DIR=/home/dlstreamer
EXTRA_PARAMS=""

VIDEO_GROUP_ID=$(getent group video | awk -F: '{printf "%s\n", $3}')
if [[ -n "$VIDEO_GROUP_ID" ]]; then
    EXTRA_PARAMS+="--group-add $VIDEO_GROUP_ID "
else
    printf "\nWARNING: video group wasn't found! GPU device(s) probably won't work inside the Docker image.\n\n";
fi
RENDER_GROUP_ID=$(getent group render | awk -F: '{printf "%s\n", $3}')
if [[ -n "$RENDER_GROUP_ID" ]]; then
    EXTRA_PARAMS+="--group-add $RENDER_GROUP_ID "
fi

if [[ -n "$MODELS_PATH" ]]; then
    EXTRA_PARAMS+="-v $MODELS_PATH:$DOCKER_HOME_DIR/intel/dl_streamer/models "
    EXTRA_PARAMS+="-e MODELS_PATH=$DOCKER_HOME_DIR/intel/dl_streamer/models "
fi
if [[ -n "$VIDEO_EXAMPLES_PATH" ]]; then
    EXTRA_PARAMS+="-v $VIDEO_EXAMPLES_PATH:$DOCKER_HOME_DIR/video-examples:ro "
    EXTRA_PARAMS+="-e VIDEO_EXAMPLES_DIR=$DOCKER_HOME_DIR/video-examples "
fi

docker run -it --privileged --net=host \
    -v ~/.Xauthority:$DOCKER_HOME_DIR/.Xauthority \
    --device=/dev/dri \
    -v /tmp/.X11-unix/ \
    -e DISPLAY=$DISPLAY \
    -e HTTP_PROXY=$HTTP_PROXY \
    -e HTTPS_PROXY=$HTTPS_PROXY \
    -e http_proxy=$http_proxy \
    -e https_proxy=$https_proxy \
    $EXTRA_PARAMS \
    $IMAGE_NAME
