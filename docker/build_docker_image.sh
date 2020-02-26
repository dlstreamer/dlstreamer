#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2020 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

build_type=${1:-opensource}
tag=${2:-latest}

if [ $build_type == "opensource" ]; then
  dockerfile=Dockerfile
elif [ $build_type == "package" ]; then
  dockerfile=binaries.Dockerfile
else
  echo "Usage: ./build_docker_image.sh [BUILDTYPE] [TAG]"
  echo "ERROR: please set BUILDTYPE to on of the following: [opensource, package]"
  exit
fi

BASEDIR=$(dirname "$0")
docker build -f ${BASEDIR}/${dockerfile} -t gst-video-analytics:$tag \
    --build-arg http_proxy=${HTTP_PROXY:-$http_proxy} \
    --build-arg https_proxy=${HTTPS_PROXY:-$https_proxy} \
    ${BASEDIR}/..
