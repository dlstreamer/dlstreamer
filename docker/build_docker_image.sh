#!/bin/bash
# ==============================================================================
# Copyright (C) 2018-2019 Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================================

tag=$1

if [ -z "$tag" ]; then
    tag=latest
fi

BASEDIR=$(dirname "$0")
docker build -f ${BASEDIR}/Dockerfile -t gstreamer-plugins:$tag \
    --build-arg http_proxy=${HTTP_PROXY} \
    --build-arg https_proxy=${HTTPS_PROXY} \
    ${BASEDIR}/..
