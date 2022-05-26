#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2022 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

# Usage: $0 IMAGE_TAG

TAG=${1:-latest}
DOCKER_PRIVATE_REGISTRY=$2
BASEDIR="$(dirname "$(readlink -fm "$0")")"
CONTEXTDIR="$(dirname "$BASEDIR")"
dockerfile="Dockerfile"
IMAGE_NAME=dlstreamer

BUILD_ARGS="--build-arg DOCKER_PRIVATE_REGISTRY=${DOCKER_PRIVATE_REGISTRY} "
BUILD_ARGS+="--build-arg http_proxy=$http_proxy "
BUILD_ARGS+="--build-arg https_proxy=$https_proxy "
BUILD_ARGS+="--build-arg no_proxy=$no_proxy "
if [[ -v DLSTREAMER_APT_REPO_COMPONENT ]]; then
    BUILD_ARGS+="--build-arg DLSTREAMER_APT_REPO_COMPONENT=${DLSTREAMER_APT_REPO_COMPONENT} "
fi
BUILD_ARGS+="--build-arg INSTALL_DPCPP=false "
if [[ -v DLSTREAMER_GIT_URL ]]; then
    BUILD_ARGS+="--build-arg DLSTREAMER_GIT_URL=${DLSTREAMER_GIT_URL} "
fi
if [[ -v DLSTREAMER_APT_REPO ]]; then
    BUILD_ARGS+=$(echo --build-arg DLSTREAMER_APT_REPO=\'${DLSTREAMER_APT_REPO}\')
fi

eval "docker build -f $BASEDIR/Dockerfile -t $IMAGE_NAME:$TAG $BUILD_ARGS $CONTEXTDIR/../.."
